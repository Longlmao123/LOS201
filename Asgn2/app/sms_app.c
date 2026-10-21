/*
 * Khoi tao, chay va dung ung dung sensor monitoring.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "sms_app.h"
#include "sms_jitter.h"
#include "sms_config.h"
#include "sms_ipc.h"
#include "sensor_thread.h"
#include "processor_thread.h"
#include "alert_thread.h"
#include "logger_thread.h"

#define SMS_THREAD_STACK (256U * 1024U)
#define SMS_JITTER_CAPACITY 100000U

/*
 * Tao queue rieng cho process.
 * Unlink ngay sau khi tao: queue van ton tai den khi
 * cac descriptor dong, khong de lai ten khi process chet.
 */
static mqd_t create_queue(const char *kind, size_t message_size)
{
    struct mq_attr attributes = {0};
    char name[64];
    mqd_t queue;
    int length;

    length = snprintf(name, sizeof(name),
                      "/sms_%s_%ld", kind, (long)getpid());

    if (length < 0 || (size_t)length >= sizeof(name)) {
        errno = ENAMETOOLONG;
        return (mqd_t)-1;
    }

    attributes.mq_maxmsg = 10;
    attributes.mq_msgsize = (long)message_size;

    queue = mq_open(name, O_CREAT | O_EXCL | O_RDWR,
                    0600, &attributes);
    if (queue == (mqd_t)-1)
        return queue;

    if (mq_unlink(name) < 0) {
        int saved_errno = errno;

        mq_close(queue);
        errno = saved_errno;
        return (mqd_t)-1;
    }

    return queue;
}

/* Join loi la loi quan ly vong doi thread, khong cleanup mu. */
static void join_worker(pthread_t thread)
{
    int result = pthread_join(thread, NULL);

    if (result != 0) {
        fprintf(stderr, "pthread_join failed: %d\n", result);
        abort();
    }
}

static void wait_for_stop(AppContext *ctx, const sigset_t *signals,
                           const struct timespec *started)
{
    while (!atomic_load(&ctx->stop_requested)) {
        struct timespec timeout = {0, 100000000L};
        struct timespec now;
        int received;

        received = sigtimedwait(signals, NULL, &timeout);

        if (received == SIGINT || received == SIGTERM) {
            fprintf(stderr, "[MAIN] received signal %d\n",
                    received);
            atomic_store(&ctx->stop_requested, true);
            break;
        }

        if (received < 0 && errno != EAGAIN && errno != EINTR) {
            sms_fail(ctx, "sigtimedwait", errno);
            break;
        }

        if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
            sms_fail(ctx, "main clock_gettime", errno);
            break;
        }

        if (ctx->config.duration_sec != 0) {
            int64_t elapsed_ns =
                ((int64_t)now.tv_sec -
                 (int64_t)started->tv_sec) * INT64_C(1000000000)
                + now.tv_nsec - started->tv_nsec;

            if (elapsed_ns >=
                (int64_t)ctx->config.duration_sec *
                INT64_C(1000000000)) {
                fprintf(stderr, "[MAIN] duration reached\n");
                atomic_store(&ctx->stop_requested, true);
                break;
            }
        }
    }
}

static void print_summary(AppContext *ctx,
                          const struct timespec *started)
{
    BufferStats stats = {0};
    struct timespec finished;
    int64_t uptime_ms = -1;

    if (clock_gettime(CLOCK_MONOTONIC, &finished) < 0)
        sms_fail(ctx, "read shutdown clock", errno);
    else
        uptime_ms = sensor_diff_us(&finished, started) / 1000;

    cb_get_stats(&ctx->shared_buffer, &stats);

    printf("\n=== SMS SUMMARY ===\n");
    printf("mode=%s policy=%s interval_ms=%u\n",
           ctx->config.mock_sensor ? "MOCK" : "DRIVER",
           ctx->config.use_fifo ? "FIFO" : "OTHER",
           ctx->config.interval_ms);
    printf("uptime_ms=%" PRId64 "\n", uptime_ms);
    printf("samples_read=%" PRIu64 "\n", ctx->samples_read);
    printf("sensor_read_errors=%" PRIu64 "\n",
           ctx->sensor_read_errors);
    printf("sensor_eagain_retries=%" PRIu64 "\n",
           ctx->sensor_eagain_retries);
    printf("sensor_interrupted_reads=%" PRIu64 "\n",
           ctx->sensor_interrupted_reads);
    printf("sensor_deferred_samples=%" PRIu64 "\n",
           ctx->sensor_deferred_samples);
    printf("samples_processed=%" PRIu64 "\n",
           ctx->samples_processed);
    printf("total_produced=%" PRIu64 "\n", stats.total_produced);
    printf("total_consumed=%" PRIu64 "\n", stats.total_consumed);
    printf("overflow_count=%" PRIu64 "\n", stats.overflow_count);
    printf("buffer_remaining=%zu\n", stats.current_size);
    printf("TEMP_HIGH=%" PRIu64 "\n", ctx->temp_high_count);
    printf("HUMID_HIGH=%" PRIu64 "\n", ctx->humid_high_count);
    printf("TEMP_SPIKE=%" PRIu64 "\n", ctx->temp_spike_count);
    printf("jitter_samples=%zu capacity=%zu\n",
           ctx->jitter_count, ctx->jitter_capacity);
    printf("fatal_error=%d\n", atomic_load(&ctx->fatal_error));
    printf("=== SMS END ===\n");
}

int main(int argc, char **argv)
{
    AppContext ctx = {0};
    pthread_t threads[4];
    void *(*const workers[4])(void *) = {
        logger_thread_main,
        alert_thread_main,
        processor_thread_main,
        sensor_thread_main
    };
    const char *const names[4] = {
        "create Logger", "create Alert",
        "create Processor", "create Sensor"
    };
    pthread_attr_t attributes;
    struct timespec started;
    sigset_t signals;
    size_t created = 0;
    bool attributes_ready = false;
    bool memory_locked = false;
    int result;
    int exit_code;

    result = sms_parse_config(argc, argv, &ctx.config);
    if (result != 0)
        return result > 0 ? 0 : 2;

    atomic_init(&ctx.stop_requested, false);
    atomic_init(&ctx.fatal_error, 0);
    ctx.alarm_queue = (mqd_t)-1;
    ctx.log_queue = (mqd_t)-1;

    result = cb_init(&ctx.shared_buffer);
    if (result != 0) {
        fprintf(stderr, "cb_init failed: %d\n", result);
        return 1;
    }

    ctx.jitter_capacity = SMS_JITTER_CAPACITY;
    ctx.jitter_us = calloc(ctx.jitter_capacity,
                           sizeof(*ctx.jitter_us));
    if (ctx.jitter_us == NULL) {
        sms_fail(&ctx, "allocate jitter buffer", ENOMEM);
        goto cleanup;
    }

    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);

    /*
     * Worker ke thua signal mask.
     * Main nhan signal bang sigtimedwait,
     * khong goi mutex hay message queue trong signal handler.
     */
    result = pthread_sigmask(SIG_BLOCK, &signals, NULL);
    if (result != 0) {
        sms_fail(&ctx, "block signals", result);
        goto cleanup;
    }

    ctx.alarm_queue = create_queue("alarm", sizeof(AlarmMessage));
    if (ctx.alarm_queue == (mqd_t)-1) {
        sms_fail(&ctx, "create alarm queue", errno);
        goto cleanup;
    }

    ctx.log_queue = create_queue("log", sizeof(LogMessage));
    if (ctx.log_queue == (mqd_t)-1) {
        sms_fail(&ctx, "create log queue", errno);
        goto cleanup;
    }

    result = pthread_attr_init(&attributes);
    if (result != 0) {
        sms_fail(&ctx, "pthread_attr_init", result);
        goto cleanup;
    }
    attributes_ready = true;

    result = pthread_attr_setstacksize(&attributes,
                                       SMS_THREAD_STACK);
    if (result != 0) {
        sms_fail(&ctx, "set worker stack size", result);
        goto cleanup;
    }

    /*
     * Chay DRIVER: bat buoc khoa bo nho.
     * MOCK tren host phuc vu debug/Valgrind, khong khoa bo nho
     * va khong dung ket qua nay lam phep do RT tren ARM.
     */
    if (!ctx.config.mock_sensor) {
        if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
            sms_fail(&ctx, "mlockall", errno);
            goto cleanup;
        }
        memory_locked = true;
        fprintf(stderr, "[MAIN] mlockall OK\n");
    } else {
        fprintf(stderr,
                "[MAIN] MOCK diagnostics: memory lock disabled\n");
    }

    if (clock_gettime(CLOCK_MONOTONIC, &started) < 0) {
        sms_fail(&ctx, "read startup clock", errno);
        goto cleanup;
    }

    fprintf(stderr,
            "[MAIN] mode=%s policy=%s interval=%u ms"
            " duration=%u s log=%s\n",
            ctx.config.mock_sensor ? "MOCK" : "DRIVER",
            ctx.config.use_fifo ? "FIFO" : "OTHER",
            ctx.config.interval_ms,
            ctx.config.duration_sec,
            ctx.config.log_path);

    for (created = 0; created < 4; created++) {
        if (atomic_load(&ctx.fatal_error) != 0)
            break;

        result = pthread_create(&threads[created],
                                 &attributes,
                                 workers[created], &ctx);
        if (result != 0) {
            sms_fail(&ctx, names[created], result);
            break;
        }
    }

    if (created == 4)
        wait_for_stop(&ctx, &signals, &started);

    atomic_store(&ctx.stop_requested, true);

    /* Sensor dong buffer khi thoat. */
    if (created == 4)
        join_worker(threads[3]);
    else
        cb_close(&ctx.shared_buffer);

    /* Processor doc het buffer khi dung binh thuong. */
    if (created >= 3)
        join_worker(threads[2]);

    if (created >= 2) {
        if (atomic_load(&ctx.fatal_error) == 0) {
            AlarmMessage stop = {.type = ALARM_STOP};

            (void)sms_send_message(&ctx, ctx.alarm_queue,
                                   &stop, sizeof(stop));
        }
        join_worker(threads[1]);
    }

    /* Chi dung Logger sau khi cac thread tao log da thoat. */
    if (created >= 1) {
        if (atomic_load(&ctx.fatal_error) == 0) {
            LogMessage stop = {.type = LOG_STOP};

            (void)sms_send_message(&ctx, ctx.log_queue,
                                   &stop, sizeof(stop));
        }
        join_worker(threads[0]);
    }

    if (atomic_load(&ctx.fatal_error) == 0 &&
        sms_export_jitter(&ctx) < 0)
        sms_fail(&ctx, "export jitter", errno);

    print_summary(&ctx, &started);

cleanup:
    if (attributes_ready) {
        result = pthread_attr_destroy(&attributes);
        if (result != 0)
            sms_fail(&ctx, "pthread_attr_destroy", result);
    }

    if (memory_locked && munlockall() < 0)
        sms_fail(&ctx, "munlockall", errno);

    if (ctx.alarm_queue != (mqd_t)-1 &&
        mq_close(ctx.alarm_queue) < 0)
        sms_fail(&ctx, "close alarm queue", errno);

    if (ctx.log_queue != (mqd_t)-1 &&
        mq_close(ctx.log_queue) < 0)
        sms_fail(&ctx, "close log queue", errno);

    exit_code = atomic_load(&ctx.fatal_error) == 0 ? 0 : 1;

    free(ctx.jitter_us);
    cb_destroy(&ctx.shared_buffer);

    fprintf(stderr, "[MAIN] exit_code=%d\n", exit_code);
    return exit_code;
}
