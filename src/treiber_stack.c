#include "treiber_stack.h"

#include <stdlib.h>

/*
 * Lock-free Treiber stack with hazard pointers.
 *
 * Important:
 *   - next: link used by the active stack.
 *   - retired_next: link used by a thread's retired-node list.
 *
 * Never reuse next for the retired list. Another thread may still
 * be reading next while protecting a node with a hazard pointer.
 */

static bool slot_valid(const treiber_stack_t *stack, size_t slot)
{
    return stack != NULL &&
           slot < TS_MAX_THREADS &&
           atomic_load_explicit(&stack->registered[slot],
                                memory_order_seq_cst);
}

bool ts_init(treiber_stack_t *stack)
{
    if (stack == NULL)
        return false;

    atomic_init(&stack->head, NULL);

    for (size_t i = 0; i < TS_MAX_THREADS; ++i) {
        atomic_init(&stack->hazards[i], NULL);
        atomic_init(&stack->registered[i], false);

        stack->retired[i] = NULL;
        stack->retired_count[i] = 0;
    }

    return true;
}

bool ts_register_thread(treiber_stack_t *stack, size_t *slot_out)
{
    if (stack == NULL || slot_out == NULL)
        return false;

    for (size_t i = 0; i < TS_MAX_THREADS; ++i) {
        bool expected = false;

        if (atomic_compare_exchange_strong_explicit(
                &stack->registered[i],
                &expected,
                true,
                memory_order_seq_cst,
                memory_order_seq_cst)) {

            atomic_store_explicit(&stack->hazards[i],
                                  NULL,
                                  memory_order_seq_cst);

            *slot_out = i;
            return true;
        }
    }

    return false;
}

/*
 * Scan this slot's retired list.
 *
 * Protected nodes remain retired. Unprotected nodes can be freed.
 * The retired list is private to its owning slot, so other threads
 * must not modify this list.
 */
static void reclaim_scan(treiber_stack_t *stack, size_t slot)
{
    ts_node_t **link = &stack->retired[slot];

    while (*link != NULL) {
        ts_node_t *node = *link;
        bool protected = false;

        for (size_t i = 0; i < TS_MAX_THREADS; ++i) {
            ts_node_t *hazard =
                atomic_load_explicit(&stack->hazards[i],
                                     memory_order_seq_cst);

            if (hazard == node) {
                protected = true;
                break;
            }
        }

        if (protected) {
            link = &node->retired_next;
        } else {
            /* Unlink through the retired-list link, not stack next. */
            *link = node->retired_next;
            free(node);
            --stack->retired_count[slot];
        }
    }
}

static void retire_node(treiber_stack_t *stack,
                        size_t slot,
                        ts_node_t *node)
{
    node->retired_next = stack->retired[slot];
    stack->retired[slot] = node;
    ++stack->retired_count[slot];

    if (stack->retired_count[slot] >= TS_RECLAIM_THRESHOLD)
        reclaim_scan(stack, slot);
}

bool ts_push(treiber_stack_t *stack, const task_t *task)
{
    if (stack == NULL || task == NULL)
        return false;

    ts_node_t *node = malloc(sizeof(*node));

    if (node == NULL)
        return false;

    node->task = *task;
    node->retired_next = NULL;

    ts_node_t *old_head;

    do {
        old_head = atomic_load_explicit(&stack->head,
                                        memory_order_seq_cst);

        node->next = old_head;
    } while (!atomic_compare_exchange_weak_explicit(
                 &stack->head,
                 &old_head,
                 node,
                 memory_order_seq_cst,
                 memory_order_seq_cst));

    return true;
}

bool ts_pop(treiber_stack_t *stack,
            size_t slot,
            task_t *task_out)
{
    if (!slot_valid(stack, slot) || task_out == NULL)
        return false;

    for (;;) {
        ts_node_t *head =
            atomic_load_explicit(&stack->head,
                                 memory_order_seq_cst);

        if (head == NULL) {
            atomic_store_explicit(&stack->hazards[slot],
                                  NULL,
                                  memory_order_seq_cst);
            return false;
        }

        /*
         * Publish the hazard, then validate that head is still the
         * current stack head before dereferencing it.
         */
        atomic_store_explicit(&stack->hazards[slot],
                              head,
                              memory_order_seq_cst);

        if (head != atomic_load_explicit(&stack->head,
                                         memory_order_seq_cst)) {
            atomic_store_explicit(&stack->hazards[slot],
                                  NULL,
                                  memory_order_seq_cst);
            continue;
        }

        /* Safe to read next while this node remains hazard-protected. */
        ts_node_t *next = head->next;

        if (atomic_compare_exchange_weak_explicit(
                &stack->head,
                &head,
                next,
                memory_order_seq_cst,
                memory_order_seq_cst)) {

            *task_out = head->task;

            atomic_store_explicit(&stack->hazards[slot],
                                  NULL,
                                  memory_order_seq_cst);

            retire_node(stack, slot, head);
            return true;
        }

        atomic_store_explicit(&stack->hazards[slot],
                              NULL,
                              memory_order_seq_cst);
    }
}

void ts_unregister_thread(treiber_stack_t *stack, size_t slot)
{
    if (!slot_valid(stack, slot))
        return;

    /*
     * Call only after this worker has stopped accessing the stack
     * through this slot.
     */
    atomic_store_explicit(&stack->hazards[slot],
                          NULL,
                          memory_order_seq_cst);

    reclaim_scan(stack, slot);

    /*
     * Do not release a slot while it still owns retired nodes.
     * Another thread must not reuse a slot with a nonempty retired list.
     */
    if (stack->retired[slot] == NULL) {
        atomic_store_explicit(&stack->registered[slot],
                              false,
                              memory_order_seq_cst);
    }
}

void ts_destroy(treiber_stack_t *stack)
{
    if (stack == NULL)
        return;

    /*
     * Precondition: all worker threads have stopped and no thread
     * can access the stack or its hazard slots.
     */

    ts_node_t *node =
        atomic_exchange_explicit(&stack->head,
                                 NULL,
                                 memory_order_seq_cst);

    /* Active stack uses next. */
    while (node != NULL) {
        ts_node_t *next = node->next;
        free(node);
        node = next;
    }

    /* Retired lists use retired_next. */
    for (size_t i = 0; i < TS_MAX_THREADS; ++i) {
        node = stack->retired[i];

        while (node != NULL) {
            ts_node_t *next = node->retired_next;
            free(node);
            node = next;
        }

        stack->retired[i] = NULL;
        stack->retired_count[i] = 0;
    }
}
