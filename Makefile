CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -pthread
INCLUDES = -Iinclude

QUEUE_SRC = src/lockfree_queue.c

all: locklab mutex_benchmark os_demos co1_demo atomic_check stack_demo aba_demo

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
stack_demo: src/stack.c tests/stack_demo.c include/stack.h include/task.h
	$(CC) $(CFLAGS) $(INCLUDES) src/stack.c tests/stack_demo.c -o $@

aba_demo: tests/aba_demo.c
	$(CC) $(CFLAGS) tests/aba_demo.c -o $@


clean:
	rm -f locklab mutex_benchmark os_demos co1_demo atomic_check stack_demo \
	      aba_demo treiber_stack_test treiber_stack_stress ms_queue_stress \
	      ms_queue_asan ms_queue_tsan ms_queue_tsan_nopie ms_queue_tsan_clang


.PHONY: all clean run
treiber_stack_test: src/treiber_stack.c tests/treiber_stack_test.c include/treiber_stack.h include/task.h
	$(CC) $(CFLAGS) $(INCLUDES) src/treiber_stack.c tests/treiber_stack_test.c -o $@

treiber_stack_stress: src/treiber_stack.c tests/treiber_stack_stress.c include/treiber_stack.h include/task.h
	$(CC) $(CFLAGS) $(INCLUDES) src/treiber_stack.c tests/treiber_stack_stress.c -o $@


ms_queue_stress: src/ms_queue.c tests/ms_queue_stress.c include/ms_queue.h include/task.h
	$(CC) $(CFLAGS) $(INCLUDES) src/ms_queue.c tests/ms_queue_stress.c -o $@
