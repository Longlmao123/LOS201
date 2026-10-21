/*
 * Xu ly trung binh truot va dieu kien canh bao.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#ifndef PROCESSOR_THREAD_H
#define PROCESSOR_THREAD_H

#include "sms_app.h"

typedef struct {
    SensorData window[SMS_ROLLING_WINDOW];
    size_t next;
    size_t count;
    double temperature_sum;
    double humidity_sum;
    bool has_previous;
    float previous_temperature;
} ProcessorState;

typedef struct {
    double average_temperature;
    double average_humidity;
    AlarmMessage alarms[3];
    size_t alarm_count;
} ProcessorResult;

void processor_analyze(ProcessorState *state,
                       const SensorData *sample,
                       const AppConfig *config,
                       ProcessorResult *result);

void *processor_thread_main(void *arg);

#endif
