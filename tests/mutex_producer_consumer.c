
#include "task.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <time.h>

typedef struct {
    task_t *items;
    size_t capacity, head, tail, count;
    pthread_mutex_t mutex;
} mutex_queue_t;

typedef struct {
    mutex_queue_t *queue;
    size_t producer_id, tasks_per_producer;
} producer_arg_t;

typedef struct {
    mutex_queue_t *queue;
    size_t total_tasks, producer_count, tasks_per_producer;
} consumer_arg_t;

static _Atomic size_t produced_count, consumed_count;
static _Atomic size_t duplicate_count, invalid_id_count;
static _Atomic size_t *task_seen;

static bool queue_init(mutex_queue_t *q, size_t capacity)
{
    q->items = calloc(capacity, sizeof(task_t));
    if (!q->items) return false;

    q->capacity = capacity;
    q->head = q->tail = q->count = 0;

    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        free(q->items);
        q->items = NULL;
        return false;
    }
    return true;
}

static void queue_destroy(mutex_queue_t *q)
{
    pthread_mutex_destroy(&q->mutex);
    free(q->items);
}

static bool enqueue(mutex_queue_t *q, const task_t *task)
{
    pthread_mutex_lock(&q->mutex);

    if (q->count == q->capacity) {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }

    q->items[q->tail] = *task;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;

    pthread_mutex_unlock(&q->mutex);
    return true;
}

static bool dequeue(mutex_queue_t *q, task_t *task)
{
    pthread_mutex_lock(&q->mutex);

    if (q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }

    *task = q->items[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;

    pthread_mutex_unlock(&q->mutex);
    return true;
}

static void *producer_thread(void *ptr)
{
    producer_arg_t *arg = ptr;

    for (size_t i = 0; i < arg->tasks_per_producer; i++) {
        task_t task;
        task.id = ((uint64_t)arg->producer_id << 32) | (uint64_t)i;

        snprintf(task.data, sizeof(task.data),
                 "Task from producer %zu", arg->producer_id);

        while (!enqueue(arg->queue, &task)) sched_yield();

        atomic_fetch_add_explicit(&produced_count, 1,
                                  memory_order_relaxed);
    }
    return NULL;
}

static void *consumer_thread(void *ptr)
{
    consumer_arg_t *arg = ptr;

    for (;;) {
        if (atomic_load_explicit(&consumed_count,
                                 memory_order_relaxed) >= arg->total_tasks)
            break;

        task_t task;

        if (!dequeue(arg->queue, &task)) {
            sched_yield();
            continue;
        }

        size_t pid = (size_t)(task.id >> 32);
        size_t tid = (size_t)(task.id & UINT32_MAX);

        if (pid >= arg->producer_count ||
            tid >= arg->tasks_per_producer) {
            atomic_fetch_add_explicit(&invalid_id_count, 1,
                                      memory_order_relaxed);
        } else {
            size_t index = pid * arg->tasks_per_producer + tid;
            size_t old = atomic_fetch_add_explicit(
                &task_seen[index], 1, memory_order_relaxed);

            if (old != 0)
                atomic_fetch_add_explicit(&duplicate_count, 1,
                                          memory_order_relaxed);
        }

        atomic_fetch_add_explicit(&consumed_count, 1,
                                  memory_order_relaxed);
    }
    return NULL;
}

static bool read_number(const char *prompt, size_t *value)
{
    char line[128];
    printf("%s", prompt);
    fflush(stdout);

    if (!fgets(line, sizeof(line), stdin)) return false;

    errno = 0;
    char *end;
    unsigned long long n = strtoull(line, &end, 10);

    if (errno || end == line || n == 0 || n > SIZE_MAX)
        return false;

    while (*end == ' ' || *end == '\t' ||
           *end == '\r' || *end == '\n') end++;

    if (*end != '\0') return false;

    *value = (size_t)n;
    return true;
}

static double seconds_between(struct timespec a, struct timespec b)
{
    return (double)(b.tv_sec - a.tv_sec) +
           (double)(b.tv_nsec - a.tv_nsec) / 1e9;
}

int main(void)
{
    size_t np, nc, per, capacity;

    printf("========================================\n");
    printf("       LOCKLAB - MUTEX BASELINE\n");
    printf("========================================\n");

    if (!read_number("Number of producers: ", &np) ||
        !read_number("Number of consumers: ", &nc) ||
        !read_number("Tasks per producer: ", &per) ||
        !read_number("Queue capacity: ", &capacity)) {
        fprintf(stderr, "Invalid positive integer input.\n");
        return EXIT_FAILURE;
    }

    if (per - 1 > UINT32_MAX || np - 1 > UINT32_MAX ||
        np > SIZE_MAX / per) {
        fprintf(stderr, "Configuration is too large.\n");
        return EXIT_FAILURE;
    }

    size_t total = np * per;

    if (total > SIZE_MAX / sizeof(*task_seen) ||
        np > SIZE_MAX / sizeof(pthread_t) ||
        nc > SIZE_MAX / sizeof(pthread_t) ||
        capacity > SIZE_MAX / sizeof(task_t)) {
        fprintf(stderr, "Configuration is too large.\n");
        return EXIT_FAILURE;
    }

    mutex_queue_t queue;
    if (!queue_init(&queue, capacity)) {
        fprintf(stderr, "Queue initialization failed.\n");
        return EXIT_FAILURE;
    }

    pthread_t *producers = calloc(np, sizeof(*producers));
    pthread_t *consumers = calloc(nc, sizeof(*consumers));
    producer_arg_t *pa = calloc(np, sizeof(*pa));
    consumer_arg_t *ca = calloc(nc, sizeof(*ca));
    task_seen = calloc(total, sizeof(*task_seen));

    if (!producers || !consumers || !pa || !ca || !task_seen) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(producers); free(consumers); free(pa); free(ca);
        free(task_seen); queue_destroy(&queue);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < total; i++)
        atomic_init(&task_seen[i], 0);

    atomic_init(&produced_count, 0);
    atomic_init(&consumed_count, 0);
    atomic_init(&duplicate_count, 0);
    atomic_init(&invalid_id_count, 0);

    printf("\nTasks expected: %zu | Queue capacity: %zu\n", total, capacity);

    size_t nc_created = 0, np_created = 0;
    bool failed = false;
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t i = 0; i < nc; i++) {
        ca[i] = (consumer_arg_t){&queue, total, np, per};

        if (pthread_create(&consumers[i], NULL,
                           consumer_thread, &ca[i]) != 0) {
            failed = true;
            break;
        }
        nc_created++;
    }

    if (!failed) {
        for (size_t i = 0; i < np; i++) {
            pa[i] = (producer_arg_t){&queue, i, per};

            if (pthread_create(&producers[i], NULL,
                               producer_thread, &pa[i]) != 0) {
                failed = true;
                break;
            }
            np_created++;
        }
    }

    for (size_t i = 0; i < np_created; i++)
        pthread_join(producers[i], NULL);

    /*
     * If thread creation fails, don't leave consumers waiting forever.
     * They finish after consuming the tasks produced so far.
     */
    if (failed) {
        size_t produced = atomic_load_explicit(
            &produced_count, memory_order_relaxed);
        for (size_t i = 0; i < nc_created; i++)
            ca[i].total_tasks = produced;
    }

    for (size_t i = 0; i < nc_created; i++)
        pthread_join(consumers[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);

    size_t produced = atomic_load_explicit(&produced_count, memory_order_relaxed);
    size_t consumed = atomic_load_explicit(&consumed_count, memory_order_relaxed);
    size_t duplicates = atomic_load_explicit(&duplicate_count, memory_order_relaxed);
    size_t invalid = atomic_load_explicit(&invalid_id_count, memory_order_relaxed);
    size_t missing = 0;

    for (size_t i = 0; i < total; i++) {
        if (atomic_load_explicit(&task_seen[i], memory_order_relaxed) == 0)
            missing++;
    }

    double elapsed = seconds_between(start, end);
    double throughput = elapsed > 0 ? (double)consumed / elapsed : 0;

    bool pass = !failed && produced == total && consumed == total &&
                missing == 0 && duplicates == 0 && invalid == 0;

    printf("\n============== RESULTS ==============\n");
    printf("Tasks expected  : %zu\n", total);
    printf("Tasks produced  : %zu\n", produced);
    printf("Tasks consumed  : %zu\n", consumed);
    printf("Missing tasks   : %zu\n", missing);
    printf("Duplicate tasks : %zu\n", duplicates);
    printf("Invalid IDs     : %zu\n", invalid);
    printf("Execution time  : %.6f seconds\n", elapsed);
    printf("Throughput      : %.2f tasks/second\n", throughput);
    printf("Correctness     : %s\n", pass ? "PASS" : "FAIL");
    printf("=====================================\n");

    free(producers); free(consumers); free(pa); free(ca); free(task_seen);
    queue_destroy(&queue);

    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}
