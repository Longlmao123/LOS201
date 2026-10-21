/*
 * Doc cau hinh tu tham so dong lenh.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#ifndef SMS_CONFIG_H
#define SMS_CONFIG_H

#include "sms_app.h"

/* Tra 0: thanh cong; 1: da in help; -1: tham so sai. */
int sms_parse_config(int argc, char **argv, AppConfig *config);

#endif
