#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sms_types.h"

#define SMS_BUFFER_CAPACITY 64U

typedef struct {
    size_t current_size;
    uint64_t total_produced;
    uint64_t total_consumed;
    uint64_t overflow_count;
} BufferStats;

typedef struct {
    SensorData *items;
    size_t head;
    size_t tail;
    size_t size;
    bool closed;

    uint64_t total_produced;
    uint64_t total_consumed;
    uint64_t overflow_count;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} CircularBuffer;

/* Khoi tao: 0 thanh cong; khac 0 la ma loi. */
int cb_init(CircularBuffer *buffer);

/* Chi huy sau khi cac thread su dung buffer da join. */
void cb_destroy(CircularBuffer *buffer);

/* Push: 1 thanh cong; 0 day, bo mau; -1 da dong. */
int cb_try_push(CircularBuffer *buffer, const SensorData *sample);

/* Pop: cho khi rong; 1 co mau; 0 da dong va het mau. */
int cb_pop(CircularBuffer *buffer, SensorData *sample);

/* Ngung nhan mau moi, danh thuc consumer dang cho. */
void cb_close(CircularBuffer *buffer);

/* Lay ban sao thong ke trong khi giu mutex. */
void cb_get_stats(CircularBuffer *buffer, BufferStats *stats);

#endif
