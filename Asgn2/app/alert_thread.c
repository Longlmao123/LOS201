/*
 * Nhan canh bao qua message queue va cap nhat thong ke.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <time.h>

#include "alert_thread.h"
#include "sms_ipc.h"

/* Tra ten loai canh bao; NULL neu ma khong hop le. */
static const char *alarm_name(AlarmType type)
{
    switch (type) {
    case ALARM_TEMP_HIGH:
        return "TEMP_HIGH";
    case ALARM_HUMID_HIGH:
        return "HUMID_HIGH";
    case ALARM_TEMP_SPIKE:
        return "TEMP_SPIKE";
    default:
        return NULL;
    }
}

/*
 * Chi Alert Thread cap nhat ba bo dem canh bao.
 * Main doc cac bo dem sau khi join thread.
 */
static void alert_count(AppContext *ctx, AlarmType type)
{
    switch (type) {
    case ALARM_TEMP_HIGH:
        ctx->temp_high_count++;
        break;
    case ALARM_HUMID_HIGH:
        ctx->humid_high_count++;
        break;
    case ALARM_TEMP_SPIKE:
        ctx->temp_spike_count++;
        break;
    default:
        break;
    }
}

void *alert_thread_main(void *arg)
{
    AppContext *ctx = arg;

    if (sms_logf(ctx, "ALERT started") < 0)
        return NULL;

    for (;;) {
        AlarmMessage message = {0};
        struct timespec deadline;
        const char *name;
        ssize_t received;
        int saved_errno;

        if (atomic_load(&ctx->fatal_error) != 0)
            break;

        if (clock_gettime(CLOCK_REALTIME, &deadline) < 0) {
            sms_fail(ctx, "alert clock_gettime", errno);
            break;
        }

        deadline.tv_sec++;

        received = mq_timedreceive(
            ctx->alarm_queue,
            (char *)&message,
            sizeof(message),
            NULL,
            &deadline);

        if (received < 0) {
            saved_errno = errno;

            if (saved_errno == EINTR ||
                saved_errno == ETIMEDOUT)
                continue;

            sms_fail(ctx, "alert mq_timedreceive",
                     saved_errno);
            break;
        }

        if ((size_t)received != sizeof(message)) {
            sms_fail(ctx, "invalid alarm message size",
                     EMSGSIZE);
            break;
        }

        /* Main gui STOP sau khi Processor da ket thuc. */
        if (message.type == ALARM_STOP)
            break;

        name = alarm_name(message.type);
        if (name == NULL) {
            sms_fail(ctx, "invalid alarm type", EPROTO);
            break;
        }

        if (fprintf(
                stderr,
                "[ALERT][%" PRIu64 "] TYPE: %s"
                " VALUE: %.1f THRESHOLD: %.1f\n",
                message.timestamp_ms,
                name,
                (double)message.value,
                (double)message.threshold) < 0) {
            sms_fail(ctx, "write alert to stderr", EIO);
            break;
        }

        alert_count(ctx, message.type);

        if (sms_logf(
                ctx,
                "ALERT timestamp_ms=%" PRIu64
                " TYPE: %s VALUE: %.1f THRESHOLD: %.1f",
                message.timestamp_ms,
                name,
                (double)message.value,
                (double)message.threshold) < 0)
            break;
    }

    if (atomic_load(&ctx->fatal_error) == 0) {
        (void)sms_logf(
            ctx,
            "ALERT stopped: TEMP_HIGH=%" PRIu64
            " HUMID_HIGH=%" PRIu64
            " TEMP_SPIKE=%" PRIu64,
            ctx->temp_high_count,
            ctx->humid_high_count,
            ctx->temp_spike_count);
    }

    fprintf(stderr,
            "[ALERT] stopped: TEMP_HIGH=%" PRIu64
            " HUMID_HIGH=%" PRIu64
            " TEMP_SPIKE=%" PRIu64 "\n",
            ctx->temp_high_count,
            ctx->humid_high_count,
            ctx->temp_spike_count);

    return NULL;
}
