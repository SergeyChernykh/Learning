# Treiber stack

## Performance benchmarks

Always benchmark an optimized build. Debug-build numbers mostly measure missing
compiler optimizations rather than the stack implementation.

```bash
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_TSAN=OFF \
  -DENABLE_ASAN=OFF \
  -DBUILD_BENCHMARKS=ON
cmake --build build-release -j
./build-release/tstack_benchmarks \
  --iterations 1000000 \
  --threads 8 \
  --rounds 9 \
  --scaling
```

Use CSV output when saving a baseline before an optimization:

```bash
./build-release/tstack_benchmarks \
  --iterations 1000000 \
  --threads 8 \
  --rounds 9 \
  --scaling \
  --csv > baseline.csv
```

Run one workload with `--benchmark NAME`. The supported workloads are listed by
`--help`.

The suite reports the median, minimum, and maximum throughput across rounds, as
well as nanoseconds per operation. Push, pop, and empty-pop count one operation
per call. Push/pop pairs and mixed producer/consumer workloads count both the
push and the successful pop. With `--scaling`, `--iterations` remains the amount
of work per worker, so total work grows with the worker count.

The workloads isolate different costs:

- `sequential_push` includes allocation and publication without contention.
- `sequential_pop` includes removal and immediate reclamation.
- `sequential_push_pop` measures the complete steady-state path.
- `sequential_empty_pop` exposes the active-reader bookkeeping cost.
- `parallel_push` measures contention on `head_` while allocating nodes.
- `parallel_pop` starts prefilled and stresses removal plus reclamation.
- `parallel_push_pop` runs producers and consumers concurrently.
- `parallel_empty_pop` stresses only the shared active-reader counter.

For comparisons, keep the machine, compiler, build flags, worker counts, and
iteration counts unchanged. Close unrelated CPU-heavy programs and prefer runs
long enough that each reported round takes at least a few hundred milliseconds.
Sanitizer builds are for correctness checks, not performance measurements.
