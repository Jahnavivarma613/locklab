#include "lockfree_queue.h"

#include <stdint.h>
#include <stdlib.h>

static bool is_power_of_two(size_t value)
{
    return value >= 2 && (value & (value - 1)) == 0;
}

bool lf_queue_init(lockfree_queue_t *queue, size_t capacity)
{
    if (queue == NULL || !is_power_of_two(capacity)) {
        return false;
    }

    queue->buffer = calloc(capacity, sizeof(queue_cell_t));

    if (queue->buffer == NULL) {
        return false;
    }

    queue->capacity = capacity;
    queue->mask = capacity - 1;

    atomic_init(&queue->enqueue_pos, 0);
    atomic_init(&queue->dequeue_pos, 0);

    for (size_t i = 0; i < capacity; i++) {
        atomic_init(&queue->buffer[i].sequence, i);
    }

    return true;
}

void lf_queue_destroy(lockfree_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }

    free(queue->buffer);

    queue->buffer = NULL;
    queue->capacity = 0;
    queue->mask = 0;
}

bool lf_queue_enqueue(lockfree_queue_t *queue, const task_t *task)
{
    if (queue == NULL || task == NULL) {
        return false;
    }

    size_t position =
        atomic_load_explicit(&queue->enqueue_pos,
                             memory_order_relaxed);

    for (;;) {
        queue_cell_t *cell =
            &queue->buffer[position & queue->mask];

        size_t sequence =
            atomic_load_explicit(&cell->sequence,
                                 memory_order_acquire);

        intptr_t difference =
            (intptr_t)sequence - (intptr_t)position;

        if (difference == 0) {

            if (atomic_compare_exchange_weak_explicit(
                    &queue->enqueue_pos,
                    &position,
                    position + 1,
                    memory_order_relaxed,
                    memory_order_relaxed)) {

                cell->task = *task;

                atomic_store_explicit(
                    &cell->sequence,
                    position + 1,
                    memory_order_release);

                return true;
            }

        } else if (difference < 0) {

            return false;

        } else {

            position =
                atomic_load_explicit(&queue->enqueue_pos,
                                     memory_order_relaxed);
        }
    }
}

bool lf_queue_dequeue(lockfree_queue_t *queue, task_t *task)
{
    if (queue == NULL || task == NULL) {
        return false;
    }

    size_t position =
        atomic_load_explicit(&queue->dequeue_pos,
                             memory_order_relaxed);

    for (;;) {
        queue_cell_t *cell =
            &queue->buffer[position & queue->mask];

        size_t sequence =
            atomic_load_explicit(&cell->sequence,
                                 memory_order_acquire);

        intptr_t difference =
            (intptr_t)sequence - (intptr_t)(position + 1);

        if (difference == 0) {

            if (atomic_compare_exchange_weak_explicit(
                    &queue->dequeue_pos,
                    &position,
                    position + 1,
                    memory_order_relaxed,
                    memory_order_relaxed)) {

                *task = cell->task;

                atomic_store_explicit(
                    &cell->sequence,
                    position + queue->mask + 1,
                    memory_order_release);

                return true;
            }

        } else if (difference < 0) {

            return false;

        } else {

            position =
                atomic_load_explicit(&queue->dequeue_pos,
                                     memory_order_relaxed);
        }
    }
}
