
#include "lockfree_queue.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <time.h>

typedef struct {
    lockfree_queue_t *queue;
    size_t producer_id;
    size_t tasks_per_producer;
} producer_arg_t;

typedef struct {
    lockfree_queue_t *queue;
    size_t total_tasks;
    size_t producer_count;
    size_t tasks_per_producer;
} consumer_arg_t;

static _Atomic size_t produced_count;
static _Atomic size_t consumed_count;
static _Atomic size_t duplicate_count;
static _Atomic size_t invalid_id_count;
static _Atomic size_t *task_seen;

static void *producer_thread(void *argument)
{
    producer_arg_t *arg = argument;

    for (size_t i = 0; i < arg->tasks_per_producer; i++) {
        task_t task;

        task.id = ((uint64_t)arg->producer_id << 32) | (uint64_t)i;

        snprintf(task.data, sizeof(task.data),
                 "Task from producer %zu", arg->producer_id);

        while (!lf_queue_enqueue(arg->queue, &task)) {
            sched_yield();
        }

        atomic_fetch_add_explicit(
            &produced_count, 1, memory_order_relaxed);
    }

    return NULL;
}

static void *consumer_thread(void *argument)
{
    consumer_arg_t *arg = argument;

    for (;;) {
        size_t current = atomic_load_explicit(
            &consumed_count, memory_order_relaxed);

        if (current >= arg->total_tasks) {
            break;
        }

        task_t task;

        if (lf_queue_dequeue(arg->queue, &task)) {
            size_t producer_id = (size_t)(task.id >> 32);
            size_t task_number = (size_t)(task.id & UINT32_MAX);

            if (producer_id < arg->producer_count &&
                task_number < arg->tasks_per_producer) {

                size_t index =
                    producer_id * arg->tasks_per_producer + task_number;

                size_t previous = atomic_fetch_add_explicit(
                    &task_seen[index], 1, memory_order_relaxed);

                if (previous != 0) {
                    atomic_fetch_add_explicit(
                        &duplicate_count, 1, memory_order_relaxed);
                }
            } else {
                atomic_fetch_add_explicit(
                    &invalid_id_count, 1, memory_order_relaxed);
            }

            atomic_fetch_add_explicit(
                &consumed_count, 1, memory_order_relaxed);
        } else {
            sched_yield();
        }
    }

    return NULL;
}

static bool read_positive_size(const char *prompt, size_t *value)
{
    char input[128];

    printf("%s", prompt);
    fflush(stdout);

    if (fgets(input, sizeof(input), stdin) == NULL) {
        return false;
    }

    if (strchr(input, '\n') == NULL && !feof(stdin)) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {}
        return false;
    }

    errno = 0;
    char *end = NULL;
    unsigned long long number = strtoull(input, &end, 10);

    if (errno != 0 || end == input || number == 0) {
        return false;
    }

    while (*end == ' ' || *end == '\t' ||
           *end == '\n' || *end == '\r') {
        end++;
    }

    if (*end != '\0' || number > SIZE_MAX) {
        return false;
    }

    *value = (size_t)number;
    return true;
}

static bool is_power_of_two(size_t value)
{
    return value >= 2 && (value & (value - 1)) == 0;
}

static double elapsed_seconds(
    const struct timespec *start,
    const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

int main(void)
{
    size_t producer_count;
    size_t consumer_count;
    size_t tasks_per_producer;
    size_t queue_capacity;

    printf("========================================\n");
    printf("              LOCKLAB\n");
    printf("    Lock-Free Producer-Consumer System\n");
    printf("========================================\n\n");
    printf("Enter runtime configuration:\n\n");

    if (!read_positive_size("Number of producers: ", &producer_count) ||
        !read_positive_size("Number of consumers: ", &consumer_count) ||
        !read_positive_size("Tasks per producer: ", &tasks_per_producer) ||
        !read_positive_size("Queue capacity: ", &queue_capacity)) {
        fprintf(stderr, "\nInvalid input. Enter positive integer values.\n");
        return EXIT_FAILURE;
    }

    if (!is_power_of_two(queue_capacity)) {
        fprintf(stderr,
                "Queue capacity must be a power of two (2, 4, 8, 16, ...).\n");
        return EXIT_FAILURE;
    }

    if (producer_count > SIZE_MAX / tasks_per_producer) {
        fprintf(stderr, "Total task count is too large.\n");
        return EXIT_FAILURE;
    }

    if (tasks_per_producer - 1 > UINT32_MAX ||
        producer_count - 1 > UINT32_MAX) {
        fprintf(stderr, "Task ID fields support at most 2^32 values.\n");
        return EXIT_FAILURE;
    }

    size_t total_tasks = producer_count * tasks_per_producer;

    if (total_tasks > SIZE_MAX / sizeof(*task_seen) ||
        producer_count > SIZE_MAX / sizeof(pthread_t) ||
        consumer_count > SIZE_MAX / sizeof(pthread_t) ||
        producer_count > SIZE_MAX / sizeof(producer_arg_t) ||
        consumer_count > SIZE_MAX / sizeof(consumer_arg_t)) {
        fprintf(stderr, "Requested configuration is too large.\n");
        return EXIT_FAILURE;
    }

    printf("\n========================================\n");
    printf("          CONFIGURATION\n");
    printf("========================================\n");
    printf("Producers        : %zu\n", producer_count);
    printf("Consumers        : %zu\n", consumer_count);
    printf("Tasks/Producer   : %zu\n", tasks_per_producer);
    printf("Queue Capacity   : %zu\n", queue_capacity);
    printf("Total Tasks      : %zu\n", total_tasks);

    lockfree_queue_t queue;

    if (!lf_queue_init(&queue, queue_capacity)) {
        fprintf(stderr, "Failed to initialize lock-free queue.\n");
        return EXIT_FAILURE;
    }

    pthread_t *producers = calloc(producer_count, sizeof(*producers));
    pthread_t *consumers = calloc(consumer_count, sizeof(*consumers));
    producer_arg_t *producer_args =
        calloc(producer_count, sizeof(*producer_args));
    consumer_arg_t *consumer_args =
        calloc(consumer_count, sizeof(*consumer_args));
    task_seen = calloc(total_tasks, sizeof(*task_seen));

    if (!producers || !consumers || !producer_args ||
        !consumer_args || !task_seen) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(producers);
        free(consumers);
        free(producer_args);
        free(consumer_args);
        free(task_seen);
        lf_queue_destroy(&queue);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < total_tasks; i++) {
        atomic_init(&task_seen[i], 0);
    }

    atomic_init(&produced_count, 0);
    atomic_init(&consumed_count, 0);
    atomic_init(&duplicate_count, 0);
    atomic_init(&invalid_id_count, 0);

    printf("\nStarting producers and consumers...\n");

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    size_t consumers_created = 0;
    size_t producers_created = 0;
    bool thread_error = false;

    for (size_t i = 0; i < consumer_count; i++) {
        consumer_args[i].queue = &queue;
        consumer_args[i].total_tasks = total_tasks;
        consumer_args[i].producer_count = producer_count;
        consumer_args[i].tasks_per_producer = tasks_per_producer;

        if (pthread_create(&consumers[i], NULL, consumer_thread,
                           &consumer_args[i]) != 0) {
            fprintf(stderr, "Failed to create consumer thread.\n");
            thread_error = true;
            break;
        }
        consumers_created++;
    }

    if (!thread_error) {
        for (size_t i = 0; i < producer_count; i++) {
            producer_args[i].queue = &queue;
            producer_args[i].producer_id = i;
            producer_args[i].tasks_per_producer = tasks_per_producer;

            if (pthread_create(&producers[i], NULL, producer_thread,
                               &producer_args[i]) != 0) {
                fprintf(stderr, "Failed to create producer thread.\n");
                thread_error = true;
                break;
            }
            producers_created++;
        }
    }

    for (size_t i = 0; i < producers_created; i++) {
        pthread_join(producers[i], NULL);
    }

    /*
     * If all producers were created, they have now completed.
     * Consumers can finish once all expected tasks are consumed.
     * On thread-creation failure, terminate the consumers safely by
     * setting their target to the number of tasks actually produced.
     */
    if (thread_error) {
        size_t actual = atomic_load_explicit(
            &produced_count, memory_order_relaxed);

        for (size_t i = 0; i < consumers_created; i++) {
            consumer_args[i].total_tasks =
                atomic_load_explicit(&consumed_count, memory_order_relaxed) +
                (actual - atomic_load_explicit(
                    &consumed_count, memory_order_relaxed));
        }
    }

    for (size_t i = 0; i < consumers_created; i++) {
        pthread_join(consumers[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);

    size_t produced = atomic_load_explicit(
        &produced_count, memory_order_relaxed);
    size_t consumed = atomic_load_explicit(
        &consumed_count, memory_order_relaxed);
    size_t duplicates = atomic_load_explicit(
        &duplicate_count, memory_order_relaxed);
    size_t invalid_ids = atomic_load_explicit(
        &invalid_id_count, memory_order_relaxed);

    size_t missing = 0;
    for (size_t i = 0; i < total_tasks; i++) {
        if (atomic_load_explicit(&task_seen[i],
                                 memory_order_relaxed) == 0) {
            missing++;
        }
    }

    double elapsed = elapsed_seconds(&start_time, &end_time);
    double throughput = elapsed > 0.0 ? (double)consumed / elapsed : 0.0;

    bool pass = !thread_error &&
                produced == total_tasks &&
                consumed == total_tasks &&
                duplicates == 0 &&
                invalid_ids == 0 &&
                missing == 0;

    printf("\n========================================\n");
    printf("                 RESULTS\n");
    printf("========================================\n");
    printf("Tasks expected  : %zu\n", total_tasks);
    printf("Tasks produced  : %zu\n", produced);
    printf("Tasks consumed  : %zu\n", consumed);
    printf("Missing tasks   : %zu\n", missing);
    printf("Duplicate tasks : %zu\n", duplicates);
    printf("Invalid IDs     : %zu\n", invalid_ids);
    printf("Execution time  : %.6f seconds\n", elapsed);
    printf("Throughput      : %.2f tasks/second\n", throughput);
    printf("Correctness     : %s\n", pass ? "PASS" : "FAIL");
    printf("========================================\n");

    free(producers);
    free(consumers);
    free(producer_args);
    free(consumer_args);
    free(task_seen);
    lf_queue_destroy(&queue);

    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}
