/*
 * Tinh trung binh 10 mau va tao cac thong diep canh bao.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#include <math.h>
#include <string.h>

#include "processor_thread.h"

/* Them mot canh bao; moi mau co toi da ba loai. */
static void processor_add_alarm(ProcessorResult *result,
                                AlarmType type,
                                uint64_t timestamp_ms,
                                float value,
                                float threshold)
{
    AlarmMessage *alarm = &result->alarms[result->alarm_count];

    alarm->type = type;
    alarm->timestamp_ms = timestamp_ms;
    alarm->value = value;
    alarm->threshold = threshold;
    result->alarm_count++;
}

/*
 * State phai duoc khoi tao bang 0 truoc mau dau tien.
 * Ham chi duoc Processor Thread su dung.
 */
void processor_analyze(ProcessorState *state,
                       const SensorData *sample,
                       const AppConfig *config,
                       ProcessorResult *result)
{
    float difference;

    memset(result, 0, sizeof(*result));

    /* Khi du 10 mau, loai mau cu nhat khoi tong. */
    if (state->count == SMS_ROLLING_WINDOW) {
        state->temperature_sum -=
            state->window[state->next].temperature;
        state->humidity_sum -=
            state->window[state->next].humidity;
    } else {
        state->count++;
    }

    state->window[state->next] = *sample;
    state->temperature_sum += sample->temperature;
    state->humidity_sum += sample->humidity;

    state->next = (state->next + 1U) % SMS_ROLLING_WINDOW;

    result->average_temperature =
        state->temperature_sum / (double)state->count;
    result->average_humidity =
        state->humidity_sum / (double)state->count;

    /* Canh bao dua tren gia tri mau, khong phai trung binh. */
    if (sample->temperature > config->temperature_threshold) {
        processor_add_alarm(result, ALARM_TEMP_HIGH,
                            sample->timestamp_ms,
                            sample->temperature,
                            config->temperature_threshold);
    }

    if (sample->humidity > config->humidity_threshold) {
        processor_add_alarm(result, ALARM_HUMID_HIGH,
                            sample->timestamp_ms,
                            sample->humidity,
                            config->humidity_threshold);
    }

    /* Mau dau tien chua co mau truoc de so sanh. */
    if (state->has_previous) {
        /*
         * Driver va MOCK co do phan giai 0.1 C.
         * Tru theo don vi 0.1 C de 20.2 - 15.2 khong bi
         * sai so float bien thanh gia tri lon hon 5 C.
         */
        difference = fabsf(roundf(sample->temperature * 10.0f) -
                           roundf(state->previous_temperature * 10.0f))
                     / 10.0f;

        if (difference > 5.0f) {
            processor_add_alarm(result, ALARM_TEMP_SPIKE,
                                sample->timestamp_ms,
                                difference, 5.0f);
        }
    }

    state->previous_temperature = sample->temperature;
    state->has_previous = true;
}

#include <inttypes.h>
#include <stdio.h>

#include "sms_ipc.h"

/*
 * Lay mau cho den khi buffer dong va da duoc doc het.
 * Khi dung binh thuong, van xu ly cac mau con trong buffer.
 */
void *processor_thread_main(void *arg)
{
    AppContext *ctx = arg;
    ProcessorState state = {0};
    SensorData sample;

    if (sms_logf(ctx, "PROCESSOR started") < 0)
        return NULL;

    while (cb_pop(&ctx->shared_buffer, &sample) == 1) {
        ProcessorResult result;
        size_t i;

        if (atomic_load(&ctx->fatal_error) != 0)
            break;

        processor_analyze(&state, &sample,
                           &ctx->config, &result);

        ctx->samples_processed++;

        for (i = 0; i < result.alarm_count; i++) {
            if (sms_send_message(ctx, ctx->alarm_queue,
                                  &result.alarms[i],
                                  sizeof(result.alarms[i])) < 0)
                break;
        }

        if (atomic_load(&ctx->fatal_error) != 0)
            break;

        if (sms_logf(
                ctx,
                "SAMPLE timestamp_ms=%" PRIu64
                " temperature=%.1f humidity=%.1f"
                " average_temperature=%.2f"
                " average_humidity=%.2f"
                " window_count=%zu alarms=%zu",
                sample.timestamp_ms,
                (double)sample.temperature,
                (double)sample.humidity,
                result.average_temperature,
                result.average_humidity,
                state.count,
                result.alarm_count) < 0)
            break;

        if (ctx->config.verbose) {
            printf("[PROCESSOR] timestamp_ms=%" PRIu64
                   " avg_temp=%.2f avg_humidity=%.2f"
                   " alarms=%zu\n",
                   sample.timestamp_ms,
                   result.average_temperature,
                   result.average_humidity,
                   result.alarm_count);
        }
    }

    if (atomic_load(&ctx->fatal_error) == 0) {
        (void)sms_logf(ctx,
                      "PROCESSOR stopped: processed=%" PRIu64,
                      ctx->samples_processed);
    }

    fprintf(stderr,
            "[PROCESSOR] stopped: processed=%" PRIu64 "\n",
            ctx->samples_processed);

    return NULL;
}
