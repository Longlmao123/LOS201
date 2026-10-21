/*
 * Muc dich: Parse mau cam bien va ho tro timing Sensor Thread.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "../driver/sms_sensor.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>

#include "sensor_thread.h"

/*
 * CSV: timestamp_ms,temperature,humidity\n
 * Chi ghi vao sample sau khi tat ca truong da hop le.
 */
int sensor_parse_csv(const char *line, SensorData *sample)
{
    SensorData parsed;
    unsigned long long timestamp;
    const char *cursor;
    char *end;

    if (line == NULL || sample == NULL ||
        !isdigit((unsigned char)line[0]))
        return -1;

    errno = 0;
    timestamp = strtoull(line, &end, 10);
    if (errno != 0 || end == line || *end != ',')
        return -1;

    parsed.timestamp_ms = (uint64_t)timestamp;
    cursor = end + 1;

    errno = 0;
    parsed.temperature = strtof(cursor, &end);
    if (errno != 0 || end == cursor || *end != ',' ||
        !isfinite(parsed.temperature) ||
        parsed.temperature < 15.0f ||
        parsed.temperature > 45.0f)
        return -1;

    cursor = end + 1;

    errno = 0;
    parsed.humidity = strtof(cursor, &end);
    if (errno != 0 || end == cursor ||
        !isfinite(parsed.humidity) ||
        parsed.humidity < 30.0f ||
        parsed.humidity > 90.0f)
        return -1;

    if (*end != '\n' || end[1] != '\0')
        return -1;

    *sample = parsed;
    return 0;
}

/* Tach giay va nanosecond de tranh tran so tren ARM 32-bit. */
void sensor_add_ms(struct timespec *time, unsigned int ms)
{
    time->tv_sec += ms / 1000U;
    time->tv_nsec += (long)(ms % 1000U) * 1000000L;

    if (time->tv_nsec >= 1000000000L) {
        time->tv_sec++;
        time->tv_nsec -= 1000000000L;
    }
}

/* Tinh hieu bang nanosecond truoc khi doi sang microsecond. */
int64_t sensor_diff_us(const struct timespec *actual,
                       const struct timespec *expected)
{
    int64_t delta_ns;

    delta_ns =
        ((int64_t)actual->tv_sec - (int64_t)expected->tv_sec)
        * INT64_C(1000000000);

    delta_ns +=
        (int64_t)actual->tv_nsec - (int64_t)expected->tv_nsec;

    return delta_ns / INT64_C(1000);
}

/* Neu ioctl that bai, dong fd va giu nguyen errno. */
int sensor_open_device(const char *path, unsigned int interval_ms)
{
    __u32 interval = interval_ms;
    int fd;
    int saved_errno;

    if (path == NULL || interval_ms == 0 ||
        interval_ms > 60000U) {
        errno = EINVAL;
        return -1;
    }

    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
        return -1;

    if (ioctl(fd, SMS_SET_INTERVAL, &interval) < 0) {
        saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    return fd;
}

/*
 * Buffer 128 byte dap ung yeu cau toi thieu 64 byte cua driver.
 * EAGAIN/EINTR/EOF va CSV hong duoc bao ve cho vong lap thu lai.
 */
int sensor_read_device(int fd, SensorData *sample)
{
    char line[128];
    ssize_t length;

    if (sample == NULL) {
        errno = EINVAL;
        return -1;
    }

    length = read(fd, line, sizeof(line) - 1);

    if (length < 0) {
        if (errno == EAGAIN || errno == EINTR)
            return 0;
        return -1;
    }

    if (length == 0) {
        errno = ENODATA;
        return 0;
    }

    line[length] = '\0';

    if (sensor_parse_csv(line, sample) != 0) {
        errno = EBADMSG;
        return 0;
    }

    return 1;
}

#include <inttypes.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>

#include "sms_app.h"
#include "sms_ipc.h"

/* So sanh hai moc thoi gian monotonic. */
static bool sensor_before(const struct timespec *a,
                          const struct timespec *b)
{
    return a->tv_sec < b->tv_sec ||
           (a->tv_sec == b->tv_sec && a->tv_nsec < b->tv_nsec);
}

/*
 * Cho theo moc tuyet doi, kiem tra yeu cau dung moi 100 ms.
 * Tra 0 khi den han; ECANCELED khi duoc yeu cau dung.
 */
static int sensor_wait_until(AppContext *ctx,
                             const struct timespec *deadline)
{
    while (!atomic_load(&ctx->stop_requested)) {
        struct timespec now;
        struct timespec wake;
        int error;

        if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
            return errno;

        if (!sensor_before(&now, deadline))
            return 0;

        wake = now;
        sensor_add_ms(&wake, 100);

        if (sensor_before(deadline, &wake))
            wake = *deadline;

        error = clock_nanosleep(CLOCK_MONOTONIC,
                                TIMER_ABSTIME, &wake, NULL);

        if (error != 0 && error != EINTR)
            return error;
    }

    return ECANCELED;
}

/* Bao loi nghiem trong cho Main va cac thread khac. */
static void sensor_failure(AppContext *ctx,
                           const char *operation, int error)
{
    int expected = 0;

    if (error == 0)
        error = EIO;

    fprintf(stderr, "[SENSOR ERROR] %s: %s\n",
            operation, strerror(error));

    atomic_compare_exchange_strong(&ctx->fatal_error,
                                    &expected, error);
    atomic_store(&ctx->stop_requested, true);
}

/* Sensor la producer duy nhat cua shared_buffer. */
void *sensor_thread_main(void *arg)
{
    AppContext *ctx = arg;
    struct sched_param parameters = {0};
    struct timespec expected;
    uint64_t skipped_periods = 0;
    int policy;
    int error;
    int fd = -1;

    if (ctx->config.interval_ms == 0 ||
        ctx->config.interval_ms > 60000U) {
        sensor_failure(ctx, "invalid interval", EINVAL);
        goto out;
    }

    policy = ctx->config.use_fifo ? SCHED_FIFO : SCHED_OTHER;
    parameters.sched_priority = ctx->config.use_fifo ? 50 : 0;

    error = pthread_setschedparam(pthread_self(),
                                  policy, &parameters);
    if (error != 0) {
        sensor_failure(ctx, "pthread_setschedparam", error);
        goto out;
    }

    fprintf(stderr, "[SENSOR] policy=%s priority=%d mode=%s\n",
            ctx->config.use_fifo ? "FIFO" : "OTHER",
            parameters.sched_priority,
            ctx->config.mock_sensor ? "MOCK" : "DRIVER");

    if (sms_logf(ctx, "SENSOR started: policy=%s priority=%d mode=%s interval_ms=%u",
                 ctx->config.use_fifo ? "FIFO" : "OTHER",
                 parameters.sched_priority,
                 ctx->config.mock_sensor ? "MOCK" : "DRIVER",
                 ctx->config.interval_ms) < 0)
        goto out;

    if (!ctx->config.mock_sensor) {
        fd = sensor_open_device(ctx->config.device_path,
                                ctx->config.interval_ms);
        if (fd < 0) {
            sensor_failure(ctx, "open/ioctl", errno);
            goto out;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &expected) < 0) {
        sensor_failure(ctx, "clock_gettime", errno);
        goto out;
    }

    sensor_add_ms(&expected, ctx->config.interval_ms);

    while (!atomic_load(&ctx->stop_requested)) {
        struct timespec actual;
        SensorData sample = {0};
        int received = 0;
        int saved_errno = 0;
        int pushed;

        error = sensor_wait_until(ctx, &expected);
        if (error == ECANCELED)
            break;
        if (error != 0) {
            sensor_failure(ctx, "clock_nanosleep", error);
            break;
        }

        if (clock_gettime(CLOCK_MONOTONIC, &actual) < 0) {
            sensor_failure(ctx, "clock_gettime", errno);
            break;
        }

        if (ctx->jitter_us != NULL &&
            ctx->jitter_count < ctx->jitter_capacity) {
            ctx->jitter_us[ctx->jitter_count++] =
                sensor_diff_us(&actual, &expected);
        }

        if (ctx->config.mock_sensor) {
            sample.timestamp_ms =
                (uint64_t)actual.tv_sec * UINT64_C(1000) +
                (uint64_t)actual.tv_nsec / UINT64_C(1000000);

            sample.temperature = 15.0f +
                (float)((ctx->samples_read * 137U) % 301U) / 10.0f;
            sample.humidity = 30.0f +
                (float)((ctx->samples_read * 217U) % 601U) / 10.0f;
            received = 1;
        } else {
            /*
             * Thu lai trong nua chu ky, thay vi gioi han 10 lan.
             * Van giu nguyen cac moc danh thuc tuyet doi.
             */
            struct timespec read_deadline = expected;
            unsigned int retry_budget =
                ctx->config.interval_ms / 2U;

            if (retry_budget == 0)
                retry_budget = 1;

            sensor_add_ms(&read_deadline, retry_budget);

            for (;;) {
                struct timespec retry;

                if (atomic_load(&ctx->stop_requested))
                    break;

                if (clock_gettime(CLOCK_MONOTONIC, &retry) < 0) {
                    received = -1;
                    saved_errno = errno;
                    break;
                }

                if (!sensor_before(&retry, &read_deadline)) {
                    saved_errno = ETIMEDOUT;
                    break;
                }

                received = sensor_read_device(fd, &sample);
                saved_errno = errno;

                if (received < 0) {
                    ctx->sensor_read_errors++;
                    break;
                }

                if (received == 1)
                    break;

                if (saved_errno == EAGAIN) {
                    ctx->sensor_eagain_retries++;
                } else if (saved_errno == EINTR) {
                    ctx->sensor_interrupted_reads++;
                } else {
                    /* EOF hoac du lieu CSV khong hop le. */
                    ctx->sensor_read_errors++;
                    break;
                }

                if (clock_gettime(CLOCK_MONOTONIC, &retry) < 0) {
                    received = -1;
                    saved_errno = errno;
                    break;
                }

                sensor_add_ms(&retry, 1);

                if (sensor_before(&read_deadline, &retry))
                    retry = read_deadline;

                error = sensor_wait_until(ctx, &retry);
                if (error == ECANCELED)
                    break;

                if (error != 0) {
                    received = -1;
                    saved_errno = error;
                    break;
                }
            }
        }

        if (atomic_load(&ctx->stop_requested))
            break;

        if (received < 0) {
            sensor_failure(ctx, "read", saved_errno);
            break;
        }

        if (received == 0) {
            ctx->sensor_deferred_samples++;
            fprintf(stderr,
                    "[SENSOR] sample deferred, errno=%d\n",
                    saved_errno);
            if (sms_logf(ctx, "SENSOR sample deferred: errno=%d",
                         saved_errno) < 0)
                break;
        } else {
            ctx->samples_read++;
            pushed = cb_try_push(&ctx->shared_buffer, &sample);

            if (pushed < 0)
                break;

            if (pushed == 0) {
                fprintf(stderr, "[SENSOR] buffer full: sample dropped\n");
                if (sms_logf(ctx,
                             "WARN SENSOR buffer full: sample dropped timestamp_ms=%" PRIu64,
                             sample.timestamp_ms) < 0)
                    break;
            }

            if (ctx->config.verbose) {
                printf("[SENSOR] %" PRIu64 ",%.1f,%.1f\n",
                       sample.timestamp_ms,
                       (double)sample.temperature,
                       (double)sample.humidity);
            }
        }

        sensor_add_ms(&expected, ctx->config.interval_ms);

        if (clock_gettime(CLOCK_MONOTONIC, &actual) < 0) {
            sensor_failure(ctx, "clock_gettime", errno);
            break;
        }

        /* Bo cac moc da lo; giu lich theo chu ky ban dau. */
        while (!sensor_before(&actual, &expected)) {
            skipped_periods++;
            sensor_add_ms(&expected, ctx->config.interval_ms);
        }
    }

out:
    if (fd >= 0 && close(fd) < 0)
        sensor_failure(ctx, "close", errno);

    cb_close(&ctx->shared_buffer);

    if (atomic_load(&ctx->fatal_error) == 0)
        (void)sms_logf(ctx,
                      "SENSOR stopped: samples=%" PRIu64
                      " skipped_periods=%" PRIu64,
                      ctx->samples_read, skipped_periods);

    fprintf(stderr,
            "[SENSOR] stopped: samples=%" PRIu64
            " skipped_periods=%" PRIu64 "\n",
            ctx->samples_read, skipped_periods);

    return NULL;
}
