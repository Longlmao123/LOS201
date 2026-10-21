#ifndef SMS_APP_H
#define SMS_APP_H

#include <mqueue.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "circular_buffer.h"

#define SMS_DEFAULT_DEVICE "/dev/sms_sensor"
#define SMS_DEFAULT_LOG "/tmp/sms.log"
#define SMS_LOG_TEXT_SIZE 256U
#define SMS_ROLLING_WINDOW 10U

typedef struct {
    unsigned int interval_ms;
    unsigned int duration_sec;
    float temperature_threshold;
    float humidity_threshold;
    bool verbose;
    bool use_fifo;
    bool mock_sensor;
    const char *device_path;
    const char *log_path;
} AppConfig;

typedef enum {
    ALARM_STOP = 0,
    ALARM_TEMP_HIGH,
    ALARM_HUMID_HIGH,
    ALARM_TEMP_SPIKE
} AlarmType;

typedef struct {
    AlarmType type;
    uint64_t timestamp_ms;
    float value;
    float threshold;
} AlarmMessage;

typedef enum {
    LOG_MESSAGE = 0,
    LOG_STOP
} LogType;

typedef struct {
    LogType type;
    char text[SMS_LOG_TEXT_SIZE];
} LogMessage;

typedef struct {
    AppConfig config;
    CircularBuffer shared_buffer;

    mqd_t alarm_queue;
    mqd_t log_queue;

    atomic_bool stop_requested;
    atomic_int fatal_error;

    /*
     * Moi nhom thong ke chi do thread tuong ung cap nhat.
     * Main chi doc sau khi da join thread.
     */
    uint64_t samples_read;
    uint64_t sensor_read_errors;
    uint64_t sensor_eagain_retries;
    uint64_t sensor_interrupted_reads;
    uint64_t sensor_deferred_samples;
    uint64_t samples_processed;
    uint64_t temp_high_count;
    uint64_t humid_high_count;
    uint64_t temp_spike_count;

    /* Sensor ghi; Main phan tich sau khi join. */
    int64_t *jitter_us;
    size_t jitter_count;
    size_t jitter_capacity;
} AppContext;

#endif
