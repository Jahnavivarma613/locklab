# LockLab — Operating Systems & System Programming

## Overview
LockLab is a C-based educational project demonstrating concurrent
producer-consumer processing with a bounded queue. It compares a
C11 atomic compare-and-swap (CAS) queue with a mutex-protected baseline
and includes Linux system-programming demonstrations.

## Requirements
- Linux (Ubuntu recommended)
- GCC with C11 support
- POSIX threads
- GNU Make

## Build
```bash
make clean
make
```

## Run the demonstrations
```bash
./co1_demo
./os_demos
./atomic_check
./locklab
./mutex_benchmark
```

Follow the prompts shown by the queue programs to configure each run.

## Project structure
- `src/lockfree_queue.c` — bounded lock-free queue implementation
- `src/main.c` — basic Linux system-call demonstration
- `include/lockfree_queue.h` — queue interface and data structures
- `include/task.h` — task structure
- `tests/producer_consumer.c` — multithreaded lock-free queue test
- `tests/mutex_producer_consumer.c` — mutex-based comparison
- `tests/atomic_check.c` — checks whether selected atomic types are lock-free
- `demos/os_demos.c` — process, IPC, signal, memory-allocation, and file-I/O demos
- `docs/benchmark-results.md` — recorded benchmark run

## Course outcome mapping

| CO | Current project evidence | Coverage note |
|---|---|---|
| CO-1 | `src/main.c`: process IDs, `write()`, `sleep()` | Basic system-call demonstration |
| CO-2 | `demos/os_demos.c`: `fork()`, `waitpid()`, child exit status | Basic process creation and synchronization |
| CO-3 | `demos/os_demos.c`: anonymous pipe and `SIGUSR1` handler | Basic IPC and signal handling |
| CO-4 | `demos/os_demos.c`: `malloc()` and `free()` | Dynamic allocation only |
| CO-5 | `demos/os_demos.c`: `open()`, `read()`, `write()`, `close()` | Basic file I/O only |
| CO-6 | Queue, pthread producer-consumer tests, mutex baseline, C11 atomics | Main project focus: concurrent queue and synchronization |

This mapping describes implemented demonstrations, not complete coverage
of every topic in the course syllabus.

## Benchmark
See `docs/benchmark-results.md`. Recorded results are from a single run;
repeat runs under comparable conditions are needed for a reliable
performance comparison.

## Limitations
The current demos do not fully demonstrate every syllabus topic. Examples
not yet implemented include FIFOs and job control, virtual-memory page
tables and page faults, copy-on-write, `mmap()`, VFS/inodes, condition
variables, semaphores, and dedicated deadlock experiments.

## Clean generated binaries
```bash
make clean
```
