#ifndef SENSOR_THREAD_H
#define SENSOR_THREAD_H

#include <stdint.h>
#include <time.h>

#include "sms_types.h"

/* Parse thanh cong tra 0; du lieu khong hop le tra -1. */
int sensor_parse_csv(const char *line, SensorData *sample);

/* Cong interval vao moc thoi gian tuyet doi. */
void sensor_add_ms(struct timespec *time, unsigned int ms);

/* actual - expected, don vi microsecond. */
int64_t sensor_diff_us(const struct timespec *actual,
                       const struct timespec *expected);

/* Ham chay cua Sensor Thread, se trien khai tiep. */
void *sensor_thread_main(void *arg);


/* Mo device va dat interval; tra fd, hoac -1 va errno. */
int sensor_open_device(const char *path, unsigned int interval_ms);

/* Doc: 1 co mau; 0 can thu lai; -1 loi nghiem trong. */
int sensor_read_device(int fd, SensorData *sample);

#endif
