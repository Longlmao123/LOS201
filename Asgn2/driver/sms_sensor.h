#ifndef SMS_SENSOR_H
#define SMS_SENSOR_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define SMS_DEVICE_PATH "/dev/sms_sensor"
#define SMS_DEFAULT_INTERVAL_MS 100U

/* Userspace gui khoang doc, don vi millisecond. */
#define SMS_SET_INTERVAL _IOW('S', 1, __u32)

#endif


