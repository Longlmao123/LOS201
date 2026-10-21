/*
 * Muc dich: Kiem tra bien va producer/consumer dong thoi.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#include <assert.h>
#include <inttypes.h>
#include <sched.h>
#include <stdio.h>

#include "circular_buffer.h"

#define TEST_SAMPLES 10000U

static CircularBuffer buffer;

/* Xac nhan cac mau nhan duoc dung thu tu, khong mat/trung. */
static void *consumer(void *arg)
{
    SensorData sample;
    uint64_t expected = 0;

    (void)arg;

    while (cb_pop(&buffer, &sample)) {
        assert(sample.timestamp_ms == expected);
        expected++;
    }

    assert(expected == TEST_SAMPLES);
    return NULL;
}

int main(void)
{
    SensorData sample = {0};
    SensorData output;
    BufferStats stats;
    pthread_t thread;

    assert(cb_init(&buffer) == 0);

    /* Lap day 64 cho; mau thu 65 phai bi tu choi. */
    for (uint64_t i = 0; i < SMS_BUFFER_CAPACITY; i++) {
        sample.timestamp_ms = i;
        assert(cb_try_push(&buffer, &sample) == 1);
    }

    assert(cb_try_push(&buffer, &sample) == 0);
    cb_get_stats(&buffer, &stats);
    assert(stats.current_size == SMS_BUFFER_CAPACITY);
    assert(stats.overflow_count == 1);

    /* Lay 32 mau roi them 32 mau de kiem tra quay vong. */
    for (uint64_t i = 0; i < 32; i++) {
        assert(cb_pop(&buffer, &output) == 1);
        assert(output.timestamp_ms == i);
    }

    for (uint64_t i = 64; i < 96; i++) {
        sample.timestamp_ms = i;
        assert(cb_try_push(&buffer, &sample) == 1);
    }

    /* Dong buffer nhung van phai lay het du lieu con lai. */
    cb_close(&buffer);
    assert(cb_try_push(&buffer, &sample) == -1);

    for (uint64_t i = 32; i < 96; i++) {
        assert(cb_pop(&buffer, &output) == 1);
        assert(output.timestamp_ms == i);
    }

    assert(cb_pop(&buffer, &output) == 0);
    cb_get_stats(&buffer, &stats);
    assert(stats.current_size == 0);
    assert(stats.total_produced == 96);
    assert(stats.total_consumed == 96);
    cb_destroy(&buffer);

    puts("PASS: full, overflow, wrap-around, close and drain");

    /* Producer va consumer truy cap buffer dong thoi. */
    assert(cb_init(&buffer) == 0);
    assert(pthread_create(&thread, NULL, consumer, NULL) == 0);

    for (uint64_t i = 0; i < TEST_SAMPLES; i++) {
        int result;

        sample.timestamp_ms = i;
        do {
            result = cb_try_push(&buffer, &sample);
            assert(result >= 0);
            if (result == 0)
                sched_yield();
        } while (result == 0);
    }

    cb_close(&buffer);
    assert(pthread_join(thread, NULL) == 0);

    cb_get_stats(&buffer, &stats);
    assert(stats.current_size == 0);
    assert(stats.total_produced == TEST_SAMPLES);
    assert(stats.total_consumed == TEST_SAMPLES);

    printf("PASS: concurrent produced=%" PRIu64
           " consumed=%" PRIu64 " remaining=%zu\n",
           stats.total_produced, stats.total_consumed,
           stats.current_size);

    cb_destroy(&buffer);
    puts("=== CIRCULAR BUFFER TEST PASSED ===");
    return 0;
}
