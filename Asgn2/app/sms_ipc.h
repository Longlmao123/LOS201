/*
 * Ham dung chung cho message queue va bao loi.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#ifndef SMS_IPC_H
#define SMS_IPC_H

#include "sms_app.h"

void sms_fail(AppContext *ctx,
              const char *operation,
              int error_code);

int sms_send_message(AppContext *ctx,
                     mqd_t queue,
                     const void *message,
                     size_t size);

int sms_logf(AppContext *ctx, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

#endif
