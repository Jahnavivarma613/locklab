# LockLab Benchmark Results

## Test Configuration
- Producers: 4
- Consumers: 4
- Tasks per producer: 10000
- Total tasks: 40000
- Queue capacity: 1024

## Results

| Metric | Lock-Free Queue | Mutex Baseline |
|---|---:|---:|
| Tasks produced | 40000 | 40000 |
| Tasks consumed | 40000 | 40000 |
| Missing tasks | 0 | 0 |
| Duplicate tasks | 0 | 0 |
| Invalid IDs | 0 | 0 |
| Execution time | 0.013490 s | 0.013233 s |
| Throughput | 2965212.79 tasks/s | 3022691.80 tasks/s |
| Correctness | PASS | PASS |

## Analysis
Both implementations processed all expected tasks and passed the
reported correctness checks.

In this single run, the mutex baseline was slightly faster.
Repeated runs are needed to make a reliable performance comparison.

## Conclusion
LockLab demonstrates concurrent producer-consumer queues using
C11 atomics and compare-and-swap operations, and a mutex-protected
baseline using POSIX threads.
