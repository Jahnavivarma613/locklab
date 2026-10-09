# LockLab Benchmark Results

## Test Configuration
- Producers: 4
- Consumers: 4
- Tasks per producer: 10,000
- Total tasks: 40,000
- Queue capacity: 1,024

## Results

| Metric | Lock-Free Queue | Mutex Baseline |
|---|---:|---:|
| Tasks expected | 40,000 | 40,000 |
| Tasks produced | 40,000 | 40,000 |
| Tasks consumed | 40,000 | 40,000 |
| Missing tasks | 0 | 0 |
| Duplicate tasks | 0 | 0 |
| Invalid IDs | 0 | 0 |
| Execution time | 0.010683 s | 0.014149 s |
| Throughput | 3,744,364.03 tasks/s | 2,827,073.50 tasks/s |
| Correctness | PASS | PASS |

## Analysis
Both implementations processed all expected tasks and passed the
reported correctness checks, with no missing, duplicate, or invalid IDs.

In these particular runs, the lock-free queue had higher throughput
and lower execution time than the mutex baseline. These are individual
measurements; repeated runs under comparable conditions are needed
before drawing a reliable performance conclusion.

## Conclusion
LockLab demonstrates a bounded concurrent producer-consumer queue
using C11 atomics and compare-and-swap operations, alongside a
mutex-protected baseline implemented with POSIX threads.
