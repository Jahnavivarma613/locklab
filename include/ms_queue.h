#ifndef MS_QUEUE_H
#define MS_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include "task.h"

#define MS_MAX_THREADS 64
#define MS_HAZARD_SLOTS 2
#define MS_RECLAIM_THRESHOLD 16

typedef struct ms_node {
    task_t task;
    _Atomic(struct ms_node *) next;
    struct ms_node *retired_next;
} ms_node_t;

typedef struct {
    _Atomic(ms_node_t *) head;
    _Atomic(ms_node_t *) tail;

    _Atomic(ms_node_t *) hazards[MS_MAX_THREADS][MS_HAZARD_SLOTS];
    atomic_bool registered[MS_MAX_THREADS];

    ms_node_t *retired[MS_MAX_THREADS];
    size_t retired_count[MS_MAX_THREADS];
} ms_queue_t;

bool ms_queue_init(ms_queue_t *queue);
bool ms_queue_register_thread(ms_queue_t *queue, size_t *slot);
void ms_queue_unregister_thread(ms_queue_t *queue, size_t slot);

bool ms_queue_enqueue(ms_queue_t *queue, const task_t *task,
                      size_t slot);
bool ms_queue_dequeue(ms_queue_t *queue, task_t *task,
                      size_t slot);

/* Call only after every worker thread has stopped. */
void ms_queue_destroy(ms_queue_t *queue);

#endif
