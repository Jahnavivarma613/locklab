#ifndef LOCKFREE_QUEUE_H
#define LOCKFREE_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>

#include "task.h"

typedef struct {
    _Atomic size_t sequence;
    task_t task;
} queue_cell_t;

typedef struct {
    size_t capacity;
    size_t mask;

    queue_cell_t *buffer;

    _Atomic size_t enqueue_pos;
    _Atomic size_t dequeue_pos;
} lockfree_queue_t;

/*
 * Initialize the lock-free queue.
 * Capacity must be a power of two.
 */
bool lf_queue_init(lockfree_queue_t *queue, size_t capacity);

/*
 * Destroy the queue and release its memory.
 */
void lf_queue_destroy(lockfree_queue_t *queue);

/*
 * Add a task to the queue.
 * Returns false if the queue is full.
 */
bool lf_queue_enqueue(lockfree_queue_t *queue, const task_t *task);

/*
 * Remove a task from the queue.
 * Returns false if the queue is empty.
 */
bool lf_queue_dequeue(lockfree_queue_t *queue, task_t *task);

#endif
