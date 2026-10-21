/*
 * Muc dich: Kiem tra SMS_SET_INTERVAL voi gia tri 500 ms.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "sms_sensor.h"

int main(void)
{
    __u32 interval = 500;
    int fd;
    int result;

    fd = open(SMS_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    result = ioctl(fd, SMS_SET_INTERVAL, &interval);
    if (result < 0)
        perror("SMS_SET_INTERVAL");
    else
        printf("SMS_SET_INTERVAL OK: %u ms\n", interval);

    close(fd);
    return result < 0 ? 1 : 0;
}
