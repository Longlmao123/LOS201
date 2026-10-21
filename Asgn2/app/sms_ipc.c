/*
 * Gui message co timeout va yeu cau dung khi gap loi.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "sms_ipc.h"

/*
 * Giu ma loi dau tien va danh thuc Processor.
 * Khong gui log qua queue trong ham bao loi nay.
 */
void sms_fail(AppContext *ctx,
              const char *operation,
              int error_code)
{
    int expected = 0;

    if (error_code <= 0)
        error_code = EIO;

    if (atomic_compare_exchange_strong(&ctx->fatal_error,
                                       &expected, error_code)) {
        fprintf(stderr, "[ERROR] %s: %s\n",
                operation, strerror(error_code));
    }

    atomic_store(&ctx->stop_requested, true);
    cb_close(&ctx->shared_buffer);
}

/*
 * Tra 0 khi gui thanh cong, -1 khi loi.
 * Moi lan gui cho toi da 2 giay.
 * Tat ca message dung priority 0 de giu thu tu FIFO.
 */
int sms_send_message(AppContext *ctx,
                     mqd_t queue,
                     const void *message,
                     size_t size)
{
    struct timespec deadline;
    int saved_errno;

    if (atomic_load(&ctx->fatal_error) != 0) {
        errno = ECANCELED;
        return -1;
    }

    if (clock_gettime(CLOCK_REALTIME, &deadline) < 0) {
        saved_errno = errno;
        sms_fail(ctx, "clock_gettime for mq",
                 saved_errno);
        errno = saved_errno;
        return -1;
    }

    deadline.tv_sec += 2;

    for (;;) {
        if (mq_timedsend(queue, (const char *)message,
                         size, 0, &deadline) == 0)
            return 0;

        saved_errno = errno;

        if (saved_errno == EINTR) {
            if (atomic_load(&ctx->fatal_error) != 0) {
                errno = ECANCELED;
                return -1;
            }

            /* Giu nguyen deadline, khong gia han khi bi ngat. */
            continue;
        }

        sms_fail(ctx, "mq_timedsend", saved_errno);
        errno = saved_errno;
        return -1;
    }
}

/* Dong goi noi dung text de Logger Thread ghi vao file. */
int sms_logf(AppContext *ctx, const char *format, ...)
{
    LogMessage message = {0};
    va_list args;
    int length;

    message.type = LOG_MESSAGE;

    va_start(args, format);
    length = vsnprintf(message.text,
                       sizeof(message.text),
                       format, args);
    va_end(args);

    if (length < 0) {
        sms_fail(ctx, "format log message", EIO);
        errno = EIO;
        return -1;
    }

    if ((size_t)length >= sizeof(message.text)) {
        sms_fail(ctx, "log message too long", EMSGSIZE);
        errno = EMSGSIZE;
        return -1;
    }

    return sms_send_message(ctx, ctx->log_queue,
                            &message, sizeof(message));
}
