#include "ms_queue.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_THREADS 32
#define MAX_TASKS 10000000U

typedef struct {
    ms_queue_t *queue;
    size_t slot;
    size_t id;
    size_t items_per_producer;
    size_t total_items;
    size_t producer_count;
    _Atomic int *seen;
    _Atomic int *failures;
    _Atomic size_t *producers_done;
    _Atomic size_t *popped_count;
} worker_arg_t;

static int read_number(const char *prompt, size_t min, size_t max,
                       size_t *result)
{
    char line[128];
    char *end;

    printf("%s", prompt);
    if (fgets(line, sizeof(line), stdin) == NULL)
        return 0;

    char *start = line;
    while (*start == ' ' || *start == '\t')
        ++start;

    if (*start == '-' || *start == '\n' || *start == '\0')
        return 0;

    unsigned long long value = strtoull(start, &end, 10);
    if (start == end)
        return 0;

    while (*end == ' ' || *end == '\t' ||
           *end == '\n' || *end == '\r')
        ++end;

    if (*end != '\0' || value < min || value > max)
        return 0;

    *result = (size_t)value;
    return 1;
}

static void *producer(void *arg)
{
    worker_arg_t *a = arg;

    for (size_t i = 0; i < a->items_per_producer; ++i) {
        task_t task = {0};
        task.id = (uint64_t)a->id * a->items_per_producer + i;

        if (!ms_queue_enqueue(a->queue, &task, a->slot))
            atomic_fetch_add(a->failures, 1);
    }

    atomic_fetch_add(a->producers_done, 1);
    return NULL;
}

static void *consumer(void *arg)
{
    worker_arg_t *a = arg;
    task_t task;

    for (;;) {
        if (ms_queue_dequeue(a->queue, &task, a->slot)) {
            if (task.id >= a->total_items) {
                atomic_fetch_add(a->failures, 1);
            } else {
                atomic_fetch_add(&a->seen[task.id], 1);
            }

            atomic_fetch_add(a->popped_count, 1);
        } else if (atomic_load(a->producers_done) ==
                   a->producer_count) {
            break;
        }
    }

    return NULL;
}

int main(void)
{
    size_t producer_count, consumer_count, items_per_producer;

    if (!read_number("Producer threads (1-32): ",
                     1, MAX_THREADS, &producer_count) ||
        !read_number("Consumer threads (1-32): ",
                     1, MAX_THREADS, &consumer_count) ||
        !read_number("Tasks per producer (1-10000000): ",
                     1, MAX_TASKS, &items_per_producer)) {
        fprintf(stderr, "Invalid input.\n");
        return 1;
    }

    if (items_per_producer > MAX_TASKS / producer_count) {
        fprintf(stderr, "Total tasks cannot exceed %u.\n", MAX_TASKS);
        return 1;
    }

    size_t total_items = producer_count * items_per_producer;

    _Atomic int *seen = calloc(total_items, sizeof(*seen));
    if (seen == NULL) {
        fprintf(stderr, "Could not allocate tracking memory.\n");
        return 1;
    }

    for (size_t i = 0; i < total_items; ++i)
        atomic_init(&seen[i], 0);

    _Atomic int failures;
    _Atomic size_t producers_done, popped_count;
    atomic_init(&failures, 0);
    atomic_init(&producers_done, 0);
    atomic_init(&popped_count, 0);

    ms_queue_t queue;
    if (!ms_queue_init(&queue)) {
        fprintf(stderr, "Queue initialization failed.\n");
        free(seen);
        return 1;
    }

    pthread_t producers[MAX_THREADS], consumers[MAX_THREADS];
    worker_arg_t producer_args[MAX_THREADS], consumer_args[MAX_THREADS];
    size_t slots[MAX_THREADS * 2];
    size_t registered = 0;

    for (; registered < producer_count + consumer_count; ++registered) {
        if (!ms_queue_register_thread(&queue, &slots[registered])) {
            fprintf(stderr, "Thread registration failed.\n");
            goto setup_failure;
        }
    }

    size_t producers_started = 0, consumers_started = 0;

    for (size_t i = 0; i < producer_count; ++i) {
        producer_args[i] = (worker_arg_t) {
            .queue = &queue,
            .slot = slots[i],
            .id = i,
            .items_per_producer = items_per_producer,
            .total_items = total_items,
            .producer_count = producer_count,
            .seen = seen,
            .failures = &failures,
            .producers_done = &producers_done,
            .popped_count = &popped_count
        };

        if (pthread_create(&producers[i], NULL, producer,
                           &producer_args[i]) != 0) {
            fprintf(stderr, "Could not create producer thread.\n");
            for (size_t j = 0; j < producers_started; ++j)
                pthread_join(producers[j], NULL);
            goto setup_failure;
        }
        ++producers_started;
    }

    for (size_t i = 0; i < consumer_count; ++i) {
        consumer_args[i] = (worker_arg_t) {
            .queue = &queue,
            .slot = slots[producer_count + i],
            .total_items = total_items,
            .producer_count = producer_count,
            .seen = seen,
            .failures = &failures,
            .producers_done = &producers_done,
            .popped_count = &popped_count
        };

        if (pthread_create(&consumers[i], NULL, consumer,
                           &consumer_args[i]) != 0) {
            fprintf(stderr, "Could not create consumer thread.\n");
            break;
        }
        ++consumers_started;
    }

    for (size_t i = 0; i < producers_started; ++i)
        pthread_join(producers[i], NULL);

    for (size_t i = 0; i < consumers_started; ++i)
        pthread_join(consumers[i], NULL);

    if (consumers_started != consumer_count) {
        fprintf(stderr, "Stress test incomplete: not all consumers started.\n");
        goto setup_failure;
    }

    size_t missing = 0, duplicates = 0;

    for (size_t i = 0; i < total_items; ++i) {
        int count = atomic_load(&seen[i]);
        if (count == 0)
            ++missing;
        else if (count > 1)
            ++duplicates;
    }

    printf("\n--- Michael-Scott queue stress test ---\n");
    printf("Producer threads: %zu\n", producer_count);
    printf("Consumer threads: %zu\n", consumer_count);
    printf("Tasks per producer: %zu\n", items_per_producer);
    printf("Expected tasks: %zu\n", total_items);
    printf("Popped tasks:   %zu\n", atomic_load(&popped_count));
    printf("Missing IDs:    %zu\n", missing);
    printf("Duplicate IDs:  %zu\n", duplicates);
    printf("Failures:       %d\n", atomic_load(&failures));

    for (size_t i = 0; i < registered; ++i)
        ms_queue_unregister_thread(&queue, slots[i]);

    ms_queue_destroy(&queue);
    free(seen);

    if (missing == 0 && duplicates == 0 &&
        atomic_load(&popped_count) == total_items &&
        atomic_load(&failures) == 0) {
        puts("PASS: all tasks received exactly once");
        return 0;
    }

    puts("FAIL: task loss, duplication, or operation failure detected");
    return 1;

setup_failure:
    for (size_t i = 0; i < registered; ++i)
        ms_queue_unregister_thread(&queue, slots[i]);

    ms_queue_destroy(&queue);
    free(seen);
    return 1;
}
