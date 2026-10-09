
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -pthread
INCLUDES = -Iinclude

QUEUE_SRC = src/lockfree_queue.c

all: locklab mutex_benchmark

locklab: $(QUEUE_SRC) tests/producer_consumer.c include/lockfree_queue.h include/task.h
	$(CC) $(CFLAGS) $(INCLUDES) $(QUEUE_SRC) tests/producer_consumer.c -o $@

mutex_benchmark: tests/mutex_producer_consumer.c include/task.h
	$(CC) $(CFLAGS) $(INCLUDES) tests/mutex_producer_consumer.c -o $@

clean:
	rm -f locklab mutex_benchmark atomic_check

run: locklab
	./locklab

.PHONY: all clean run
