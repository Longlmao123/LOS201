/*
 * Muc dich: Kiem tra nguong canh bao va rolling window 10 mau.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-22
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "processor_thread.h"

static int has_alarm(const ProcessorResult *result, AlarmType type)
{
    for (size_t i = 0; i < result->alarm_count; i++)
        if (result->alarms[i].type == type)
            return 1;
    return 0;
}

int main(void)
{
    AppConfig config = {.temperature_threshold = 40.0f,
                        .humidity_threshold = 80.0f};
    ProcessorState state = {0};
    ProcessorResult result;
    SensorData sample = {.timestamp_ms = 1, .temperature = 40.0f,
                         .humidity = 80.0f};

    processor_analyze(&state, &sample, &config, &result);
    assert(result.alarm_count == 0);
    sample.temperature = 40.1f;
    sample.humidity = 80.1f;
    processor_analyze(&state, &sample, &config, &result);
    assert(has_alarm(&result, ALARM_TEMP_HIGH));
    assert(has_alarm(&result, ALARM_HUMID_HIGH));
    assert(!has_alarm(&result, ALARM_TEMP_SPIKE));

    for (int direction = -1; direction <= 1; direction += 2) {
        for (int low10 = 150; low10 <= 400; low10++) {
            state = (ProcessorState){0};
            float low = (float)low10 / 10.0f;
            float high = (float)(low10 + 50) / 10.0f;
            sample.temperature = direction > 0 ? low : high;
            processor_analyze(&state, &sample, &config, &result);
            sample.temperature = direction > 0 ? high : low;
            processor_analyze(&state, &sample, &config, &result);
            assert(!has_alarm(&result, ALARM_TEMP_SPIKE));
        }
    }
    puts("PASS: 502 transitions of exactly 5.0 C do not raise TEMP_SPIKE");

    for (int direction = -1; direction <= 1; direction += 2) {
        for (int low10 = 150; low10 <= 399; low10++) {
            state = (ProcessorState){0};
            float low = (float)low10 / 10.0f;
            float high = (float)(low10 + 51) / 10.0f;
            sample.temperature = direction > 0 ? low : high;
            processor_analyze(&state, &sample, &config, &result);
            sample.temperature = direction > 0 ? high : low;
            processor_analyze(&state, &sample, &config, &result);
            assert(has_alarm(&result, ALARM_TEMP_SPIKE));
        }
    }
    puts("PASS: 500 transitions of 5.1 C raise TEMP_SPIKE");

    state = (ProcessorState){0};
    for (int i = 0; i < 11; i++) {
        sample.temperature = 15.0f + (float)i;
        sample.humidity = 40.0f + (float)i;
        processor_analyze(&state, &sample, &config, &result);
    }
    assert(state.count == SMS_ROLLING_WINDOW);
    assert(fabs(result.average_temperature - 20.5) < 1e-6);
    assert(fabs(result.average_humidity - 45.5) < 1e-6);
    puts("PASS: thresholds, first sample and rolling window after wrap-around");
    puts("=== PROCESSOR TEST PASSED ===");
    return 0;
}
