/*
 * Muc dich: Circular buffer 64 mau, bao ve bang mutex.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "circular_buffer.h"

/* Loi pthread bat thuong: bao loi va dung voi exit khac 0. */
static void check_pthread(int error)
{
    if (error != 0) {
        fprintf(stderr, "pthread error: %s\n", strerror(error));
        abort();
    }
}

/* Goi mot lan truoc khi tao cac worker thread. */
int cb_init(CircularBuffer *buffer)
{
    int error;

    if (buffer == NULL)
        return EINVAL;

    memset(buffer, 0, sizeof(*buffer));

    buffer->items = calloc(SMS_BUFFER_CAPACITY,
                           sizeof(*buffer->items));
    if (buffer->items == NULL)
        return ENOMEM;

    error = pthread_mutex_init(&buffer->mutex, NULL);
    if (error != 0) {
        free(buffer->items);
        buffer->items = NULL;
        return error;
    }

    error = pthread_cond_init(&buffer->not_empty, NULL);
    if (error != 0) {
        check_pthread(pthread_mutex_destroy(&buffer->mutex));
        free(buffer->items);
        buffer->items = NULL;
        return error;
    }

    return 0;
}

/* Chi goi sau khi tat ca thread su dung buffer da join. */
void cb_destroy(CircularBuffer *buffer)
{
    if (buffer == NULL || buffer->items == NULL)
        return;

    check_pthread(pthread_cond_destroy(&buffer->not_empty));
    check_pthread(pthread_mutex_destroy(&buffer->mutex));

    free(buffer->items);
    buffer->items = NULL;
}

/* Khong cho buffer co cho trong: day thi bo mau moi. */
int cb_try_push(CircularBuffer *buffer, const SensorData *sample)
{
    int result;

    check_pthread(pthread_mutex_lock(&buffer->mutex));

    if (buffer->closed) {
        result = -1;
    } else if (buffer->size == SMS_BUFFER_CAPACITY) {
        buffer->overflow_count++;
        result = 0;
    } else {
        buffer->items[buffer->head] = *sample;
        buffer->head =
            (buffer->head + 1) % SMS_BUFFER_CAPACITY;
        buffer->size++;
        buffer->total_produced++;

        check_pthread(pthread_cond_signal(&buffer->not_empty));
        result = 1;
    }

    check_pthread(pthread_mutex_unlock(&buffer->mutex));
    return result;
}

/* Cho khi rong; khi dong van lay het cac mau con lai. */
int cb_pop(CircularBuffer *buffer, SensorData *sample)
{
    check_pthread(pthread_mutex_lock(&buffer->mutex));

    while (buffer->size == 0 && !buffer->closed) {
        check_pthread(pthread_cond_wait(&buffer->not_empty,
                                        &buffer->mutex));
    }

    if (buffer->size == 0) {
        check_pthread(pthread_mutex_unlock(&buffer->mutex));
        return 0;
    }

    *sample = buffer->items[buffer->tail];
    buffer->tail =
        (buffer->tail + 1) % SMS_BUFFER_CAPACITY;
    buffer->size--;
    buffer->total_consumed++;

    check_pthread(pthread_mutex_unlock(&buffer->mutex));
    return 1;
}

/* Goi tu thread dieu khien shutdown, khong tu signal handler. */
void cb_close(CircularBuffer *buffer)
{
    check_pthread(pthread_mutex_lock(&buffer->mutex));

    buffer->closed = true;
    check_pthread(pthread_cond_broadcast(&buffer->not_empty));

    check_pthread(pthread_mutex_unlock(&buffer->mutex));
}

/* Lay thong ke nhat quan tai mot thoi diem. */
void cb_get_stats(CircularBuffer *buffer, BufferStats *stats)
{
    check_pthread(pthread_mutex_lock(&buffer->mutex));

    stats->current_size = buffer->size;
    stats->total_produced = buffer->total_produced;
    stats->total_consumed = buffer->total_consumed;
    stats->overflow_count = buffer->overflow_count;

    check_pthread(pthread_mutex_unlock(&buffer->mutex));
}
