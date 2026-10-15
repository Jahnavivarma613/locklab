#ifndef TREIBER_STACK_H
#define TREIBER_STACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include "task.h"

#define TS_MAX_THREADS 64
#define TS_RECLAIM_THRESHOLD 16

typedef struct ts_node {
    task_t task;
    struct ts_node *next;         /* Active stack link */
    struct ts_node *retired_next; /* Private retired-list link */
} ts_node_t;
typedef struct {
    _Atomic(ts_node_t *) head;
    _Atomic(ts_node_t *) hazards[TS_MAX_THREADS];
    atomic_bool registered[TS_MAX_THREADS];
    size_t retired_count[TS_MAX_THREADS];
    ts_node_t *retired[TS_MAX_THREADS];
} treiber_stack_t;

bool ts_init(treiber_stack_t *stack);
bool ts_register_thread(treiber_stack_t *stack, size_t *slot);
void ts_unregister_thread(treiber_stack_t *stack, size_t slot);
bool ts_push(treiber_stack_t *stack, const task_t *task);
bool ts_pop(treiber_stack_t *stack, size_t slot, task_t *task);
void ts_destroy(treiber_stack_t *stack);

#endif
