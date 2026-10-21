#ifndef SMS_TYPES_H
#define SMS_TYPES_H

#include <stdint.h>

/*
 * Mot mau cam bien sau khi parse CSV tu driver.
 * timestamp_ms: thoi gian monotonic, don vi millisecond.
 * temperature: do C; humidity: phan tram.
 */
typedef struct {
    uint64_t timestamp_ms;
    float temperature;
    float humidity;
} SensorData;

#endif
