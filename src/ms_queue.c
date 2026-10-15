#include "ms_queue.h"

#include <stdlib.h>

static bool slot_valid(const ms_queue_t *q, size_t slot)
{
    return q != NULL &&
           slot < MS_MAX_THREADS &&
           atomic_load_explicit(&q->registered[slot],
                                memory_order_seq_cst);
}

static void clear_hazards(ms_queue_t *q, size_t slot)
{
    atomic_store_explicit(&q->hazards[slot][0], NULL,
                          memory_order_seq_cst);
    atomic_store_explicit(&q->hazards[slot][1], NULL,
                          memory_order_seq_cst);
}

static bool is_hazardous(ms_queue_t *q, ms_node_t *node)
{
    for (size_t i = 0; i < MS_MAX_THREADS; ++i) {
        for (size_t j = 0; j < MS_HAZARD_SLOTS; ++j) {
            if (atomic_load_explicit(&q->hazards[i][j],
                                     memory_order_seq_cst) == node)
                return true;
        }
    }
    return false;
}

static void reclaim_scan(ms_queue_t *q, size_t slot)
{
    ms_node_t **link = &q->retired[slot];

    while (*link != NULL) {
        ms_node_t *node = *link;

        if (is_hazardous(q, node)) {
            link = &node->retired_next;
        } else {
            *link = node->retired_next;
            free(node);
            --q->retired_count[slot];
        }
    }
}

static void retire_node(ms_queue_t *q, size_t slot, ms_node_t *node)
{
    node->retired_next = q->retired[slot];
    q->retired[slot] = node;
    ++q->retired_count[slot];

    if (q->retired_count[slot] >= MS_RECLAIM_THRESHOLD)
        reclaim_scan(q, slot);
}

bool ms_queue_init(ms_queue_t *q)
{
    if (q == NULL)
        return false;

    ms_node_t *dummy = calloc(1, sizeof(*dummy));
    if (dummy == NULL)
        return false;

    atomic_init(&dummy->next, NULL);
    atomic_init(&q->head, dummy);
    atomic_init(&q->tail, dummy);

    for (size_t i = 0; i < MS_MAX_THREADS; ++i) {
        for (size_t j = 0; j < MS_HAZARD_SLOTS; ++j)
            atomic_init(&q->hazards[i][j], NULL);

        atomic_init(&q->registered[i], false);
        q->retired[i] = NULL;
        q->retired_count[i] = 0;
    }

    return true;
}

bool ms_queue_register_thread(ms_queue_t *q, size_t *slot)
{
    if (q == NULL || slot == NULL)
        return false;

    for (size_t i = 0; i < MS_MAX_THREADS; ++i) {
        bool expected = false;

        if (atomic_compare_exchange_strong_explicit(
                &q->registered[i], &expected, true,
                memory_order_seq_cst, memory_order_seq_cst)) {
            *slot = i;
            return true;
        }
    }

    return false;
}

bool ms_queue_enqueue(ms_queue_t *q, const task_t *task, size_t slot)
{
    if (!slot_valid(q, slot) || task == NULL)
        return false;

    ms_node_t *node = malloc(sizeof(*node));
    if (node == NULL)
        return false;

    node->task = *task;
    atomic_init(&node->next, NULL);
    node->retired_next = NULL;

    for (;;) {
        ms_node_t *tail =
            atomic_load_explicit(&q->tail, memory_order_seq_cst);

        atomic_store_explicit(&q->hazards[slot][0], tail,
                              memory_order_seq_cst);

        if (tail != atomic_load_explicit(&q->tail,
                                         memory_order_seq_cst))
            continue;

        ms_node_t *next =
            atomic_load_explicit(&tail->next, memory_order_seq_cst);

        atomic_store_explicit(&q->hazards[slot][1], next,
                              memory_order_seq_cst);

        if (tail != atomic_load_explicit(&q->tail,
                                         memory_order_seq_cst) ||
            next != atomic_load_explicit(&tail->next,
                                         memory_order_seq_cst))
            continue;

        if (next == NULL) {
            ms_node_t *expected = NULL;

            if (atomic_compare_exchange_weak_explicit(
                    &tail->next, &expected, node,
                    memory_order_seq_cst, memory_order_seq_cst)) {

                atomic_compare_exchange_strong_explicit(
                    &q->tail, &tail, node,
                    memory_order_seq_cst, memory_order_seq_cst);

                clear_hazards(q, slot);
                return true;
            }
        } else {
            atomic_compare_exchange_weak_explicit(
                &q->tail, &tail, next,
                memory_order_seq_cst, memory_order_seq_cst);
        }
    }
}

bool ms_queue_dequeue(ms_queue_t *q, task_t *task, size_t slot)
{
    if (!slot_valid(q, slot) || task == NULL)
        return false;

    for (;;) {
        ms_node_t *head =
            atomic_load_explicit(&q->head, memory_order_seq_cst);

        atomic_store_explicit(&q->hazards[slot][0], head,
                              memory_order_seq_cst);

        if (head != atomic_load_explicit(&q->head,
                                         memory_order_seq_cst))
            continue;

        ms_node_t *tail =
            atomic_load_explicit(&q->tail, memory_order_seq_cst);

        ms_node_t *next =
            atomic_load_explicit(&head->next, memory_order_seq_cst);

        atomic_store_explicit(&q->hazards[slot][1], next,
                              memory_order_seq_cst);

        if (head != atomic_load_explicit(&q->head,
                                         memory_order_seq_cst) ||
            next != atomic_load_explicit(&head->next,
                                         memory_order_seq_cst))
            continue;

        if (next == NULL) {
            clear_hazards(q, slot);
            return false;
        }

        if (head == tail) {
            atomic_compare_exchange_weak_explicit(
                &q->tail, &tail, next,
                memory_order_seq_cst, memory_order_seq_cst);
            continue;
        }

        /*
         * next is hazard-protected. It contains the item that
         * becomes visible when head advances to next.
         */
        task_t result = next->task;
        ms_node_t *expected = head;

        if (atomic_compare_exchange_weak_explicit(
                &q->head, &expected, next,
                memory_order_seq_cst, memory_order_seq_cst)) {
            *task = result;
            clear_hazards(q, slot);
            retire_node(q, slot, head);
            return true;
        }
    }
}

void ms_queue_unregister_thread(ms_queue_t *q, size_t slot)
{
    if (!slot_valid(q, slot))
        return;

    clear_hazards(q, slot);
    reclaim_scan(q, slot);

    /*
     * Keep the slot reserved if nodes remain on its retired list.
     * Destroy the queue after all workers have joined to reclaim
     * any remaining retired nodes.
     */
    if (q->retired[slot] == NULL)
        atomic_store_explicit(&q->registered[slot], false,
                              memory_order_seq_cst);
}

void ms_queue_destroy(ms_queue_t *q)
{
    if (q == NULL)
        return;

    /* No worker may be accessing the queue during destruction. */
    ms_node_t *node =
        atomic_load_explicit(&q->head, memory_order_seq_cst);

    while (node != NULL) {
        ms_node_t *next =
            atomic_load_explicit(&node->next, memory_order_seq_cst);
        free(node);
        node = next;
    }

    for (size_t i = 0; i < MS_MAX_THREADS; ++i) {
        node = q->retired[i];
        while (node != NULL) {
            ms_node_t *next = node->retired_next;
            free(node);
            node = next;
        }

        q->retired[i] = NULL;
        q->retired_count[i] = 0;
    }

    atomic_store_explicit(&q->head, NULL, memory_order_seq_cst);
    atomic_store_explicit(&q->tail, NULL, memory_order_seq_cst);
}        
