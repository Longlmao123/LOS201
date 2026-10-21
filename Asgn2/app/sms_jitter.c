/*
 * Muc dich: Xuat jitter theo thu tu do sau khi join cac thread.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>

#include "sms_jitter.h"

int sms_export_jitter(const AppContext *ctx)
{
    char path[PATH_MAX];
    FILE *output;
    int length;
    int saved_error = 0;

    if (ctx == NULL || ctx->config.log_path == NULL ||
        ctx->jitter_us == NULL ||
        ctx->jitter_count > ctx->jitter_capacity) {
        errno = EINVAL;
        return -1;
    }

    length = snprintf(path, sizeof(path), "%s.jitter.csv",
                      ctx->config.log_path);
    if (length < 0 || (size_t)length >= sizeof(path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    output = fopen(path, "w");
    if (output == NULL)
        return -1;

    if (fprintf(output, "sample,jitter_us\n") < 0)
        saved_error = errno ? errno : EIO;

    for (size_t i = 0;
         saved_error == 0 && i < ctx->jitter_count; i++) {
        if (fprintf(output, "%zu,%" PRId64 "\n",
                    i + 1, ctx->jitter_us[i]) < 0)
            saved_error = errno ? errno : EIO;
    }

    if (fclose(output) != 0 && saved_error == 0)
        saved_error = errno ? errno : EIO;

    if (saved_error != 0) {
        errno = saved_error;
        return -1;
    }

    fprintf(stderr,
            "[JITTER] mode=%s policy=%s interval_ms=%u"
            " samples=%zu file=%s\n",
            ctx->config.mock_sensor ? "MOCK" : "DRIVER",
            ctx->config.use_fifo ? "FIFO" : "OTHER",
            ctx->config.interval_ms,
            ctx->jitter_count, path);

    if (ctx->jitter_count == ctx->jitter_capacity)
        fprintf(stderr,
                "[JITTER] capacity reached; recording limited"
                " to the first %zu wakeups\n",
                ctx->jitter_capacity);

    return 0;
}
