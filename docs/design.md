# LockLab Design Document

## 1. Overview

LockLab is a C11/Linux concurrency project demonstrating lock-free data structures, safe memory reclamation, producer-consumer workloads, and comparison with mutex-based synchronization.

## 2. Data Structures

### Bounded MPMC Ring Buffer

The bounded multi-producer, multi-consumer queue uses atomic sequence numbers and compare-and-swap operations to coordinate access to buffer slots.

### Treiber Stack

The Treiber stack uses an atomic head pointer. Push operations publish a new node using CAS; pop operations attempt to replace the head with its successor. Hazard pointers are used to protect nodes during concurrent access.

### Michael–Scott Queue

The Michael–Scott queue maintains atomic head and tail pointers and a linked list with a dummy node. Enqueue operations link new nodes at the tail; dequeue operations advance the head. CAS operations allow threads to help advance the tail when necessary.

## 3. Memory Reclamation

The stack and Michael–Scott queue implementations use hazard-pointer slots. A thread publishes pointers it is accessing and validates the shared pointer before proceeding. Retired nodes are scanned and reclaimed only when they are not found in the hazard-pointer records.

Hazard-pointer correctness depends on the full publication, validation, scanning, and reclamation protocol. Passing stress tests or sanitizers alone is not a formal proof of correctness.

## 4. ABA Demonstration

The educational ABA demo models a thread observing pointer A, another thread changing the state from A to B and back to A, and the first thread successfully performing a stale pointer-only CAS.

A second model includes a version field. The version changes when the state changes, causing the stale CAS to fail.

This is a deterministic model, not yet a reproduction using a real concurrent Treiber stack. A production tagged-pointer implementation requires a suitable atomic representation and platform support.

## 5. Memory Ordering

The current stack and Michael–Scott queue implementations use sequentially consistent atomic operations (`memory_order_seq_cst`). This provides a single total order for their sequentially consistent atomic operations, but it does not automatically make an incorrect reclamation protocol safe.

The bounded ring buffer uses acquire/release ordering around slot sequence numbers. These operations are intended to coordinate publication and consumption of slot contents.

## 6. Testing

Recorded tests include:
- Treiber stack basic push/pop and empty-stack checks.
- Treiber stack stress test with multiple producers and consumers.
- Michael–Scott queue stress tests checking missing and duplicate task IDs.
- AddressSanitizer and UndefinedBehaviorSanitizer runs.
- A Clang ThreadSanitizer run.
- A deterministic ABA model demonstration.

Observed passing runs are evidence for the tested executions, not proof that all concurrency schedules are correct.

## 7. Known Limitations

- The ABA demo is a model rather than a real concurrent stack reproduction.
- Hazard-pointer protocols require further systematic correctness review.
- Dynamic allocation (`malloc`/`free`) may block, so the entire operation is not necessarily lock-free in the strict progress-guarantee sense.
- Benchmark comparisons require repeated runs across thread counts and comparable system conditions.
- Sanitizer success does not prove the absence of every concurrency bug.
