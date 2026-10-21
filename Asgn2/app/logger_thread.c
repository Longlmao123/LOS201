/*
 * Ghi log co timestamp, flush dinh ky va xoay file.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "logger_thread.h"
#include "sms_ipc.h"

#define SMS_LOG_LIMIT (1024L * 1024L)
#define SMS_FLUSH_SECONDS 5

/* Mo file va dat vi tri o cuoi de tinh dung kich thuoc. */
static FILE *logger_open(const char *path)
{
    FILE *file = fopen(path, "a");

    if (file == NULL)
        return NULL;

    if (fseeko(file, 0, SEEK_END) < 0) {
        int saved_errno = errno;

        fclose(file);
        errno = saved_errno;
        return NULL;
    }

    return file;
}

/* Ghi mot dong voi thoi gian lich va mui gio hien tai. */
static int logger_write(FILE *file, const char *text)
{
    struct timespec now;
    struct tm local;
    char date[32];
    char zone[16];

    if (clock_gettime(CLOCK_REALTIME, &now) < 0)
        return -1;

    if (localtime_r(&now.tv_sec, &local) == NULL)
        return -1;

    if (strftime(date, sizeof(date),
                 "%Y-%m-%d %H:%M:%S", &local) == 0 ||
        strftime(zone, sizeof(zone), "%z", &local) == 0) {
        errno = EOVERFLOW;
        return -1;
    }

    if (fprintf(file, "[%s.%03ld %s] %s\n",
                date, now.tv_nsec / 1000000L,
                zone, text) < 0)
        return -1;

    return 0;
}

/*
 * Giu mot file cu: path.1.
 * Chi Logger Thread mo, ghi va xoay file.
 */
static int logger_rotate(FILE **file, const char *path)
{
    char backup[PATH_MAX];
    off_t position;
    int length;
    int result;

    position = ftello(*file);
    if (position < 0)
        return -1;

    if (position <= SMS_LOG_LIMIT)
        return 0;

    length = snprintf(backup, sizeof(backup), "%s.1", path);
    if (length < 0 || (size_t)length >= sizeof(backup)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (fflush(*file) == EOF)
        return -1;

    result = fclose(*file);
    *file = NULL;

    if (result == EOF)
        return -1;

    if (rename(path, backup) < 0)
        return -1;

    *file = logger_open(path);
    if (*file == NULL)
        return -1;

    return 0;
}

/* So sanh moc thoi gian MONOTONIC. */
static int logger_time_reached(const struct timespec *now,
                               const struct timespec *deadline)
{
    return now->tv_sec > deadline->tv_sec ||
           (now->tv_sec == deadline->tv_sec &&
            now->tv_nsec >= deadline->tv_nsec);
}

void *logger_thread_main(void *arg)
{
    AppContext *ctx = arg;
    struct timespec next_flush;
    FILE *file;

    file = logger_open(ctx->config.log_path);
    if (file == NULL) {
        sms_fail(ctx, "open log file", errno);
        return NULL;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &next_flush) < 0) {
        sms_fail(ctx, "logger monotonic clock", errno);
        goto out;
    }
    next_flush.tv_sec += SMS_FLUSH_SECONDS;

    if (logger_write(file, "LOGGER started") < 0) {
        sms_fail(ctx, "write logger startup", errno);
        goto out;
    }

    for (;;) {
        LogMessage message = {0};
        struct timespec deadline;
        struct timespec now;
        ssize_t received;
        int saved_errno;

        if (atomic_load(&ctx->fatal_error) != 0)
            break;

        if (clock_gettime(CLOCK_REALTIME, &deadline) < 0) {
            sms_fail(ctx, "logger realtime clock", errno);
            break;
        }
        deadline.tv_sec++;

        received = mq_timedreceive(
            ctx->log_queue,
            (char *)&message,
            sizeof(message),
            NULL,
            &deadline);

        if (received < 0) {
            saved_errno = errno;

            if (saved_errno != EINTR &&
                saved_errno != ETIMEDOUT) {
                sms_fail(ctx, "logger mq_timedreceive",
                         saved_errno);
                break;
            }
        } else {
            if ((size_t)received != sizeof(message)) {
                sms_fail(ctx, "invalid log message size",
                         EMSGSIZE);
                break;
            }

            if (message.type == LOG_STOP)
                break;

            if (message.type != LOG_MESSAGE ||
                memchr(message.text, '\0',
                       sizeof(message.text)) == NULL) {
                sms_fail(ctx, "invalid log message", EPROTO);
                break;
            }

            if (logger_write(file, message.text) < 0) {
                sms_fail(ctx, "write log file", errno);
                break;
            }
        }

        /*
         * Kiem tra ca khi queue lien tuc co message,
         * khong chi khi receive timeout.
         */
        if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
            sms_fail(ctx, "logger flush clock", errno);
            break;
        }

        if (logger_time_reached(&now, &next_flush)) {
            if (fflush(file) == EOF) {
                sms_fail(ctx, "flush log file", errno);
                break;
            }

            next_flush = now;
            next_flush.tv_sec += SMS_FLUSH_SECONDS;
        }

        if (logger_rotate(&file, ctx->config.log_path) < 0) {
            sms_fail(ctx, "rotate log file", errno);
            break;
        }
    }

    if (file != NULL &&
        atomic_load(&ctx->fatal_error) == 0) {
        if (logger_write(file, "LOGGER stopped") < 0)
            sms_fail(ctx, "write logger shutdown", errno);
        else if (logger_rotate(&file,
                               ctx->config.log_path) < 0)
            sms_fail(ctx, "rotate final log", errno);
    }

out:
    if (file != NULL) {
        if (fflush(file) == EOF)
            sms_fail(ctx, "final log flush", errno);

        if (fclose(file) == EOF)
            sms_fail(ctx, "close log file", errno);
    }

    fprintf(stderr, "[LOGGER] stopped\n");
    return NULL;
}
