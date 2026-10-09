CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -pthread
INCLUDES = -Iinclude

QUEUE_SRC = src/lockfree_queue.c

all: locklab mutex_benchmark os_demos co1_demo atomic_check

locklab: $(QUEUE_SRC) tests/producer_consumer.c include/lockfree_queue.h include/task.h
	$(CC) $(CFLAGS) $(INCLUDES) $(QUEUE_SRC) tests/producer_consumer.c -o $@

mutex_benchmark: tests/mutex_producer_consumer.c include/task.h
	$(CC) $(CFLAGS) $(INCLUDES) tests/mutex_producer_consumer.c -o $@

os_demos: demos/os_demos.c
	$(CC) $(CFLAGS) demos/os_demos.c -o $@

co1_demo: src/main.c
	$(CC) $(CFLAGS) src/main.c -o $@

atomic_check: tests/atomic_check.c
	$(CC) $(CFLAGS) tests/atomic_check.c -o $@

clean:
	rm -f locklab mutex_benchmark os_demos co1_demo atomic_check

run: locklab
	./locklab

.PHONY: all clean run
