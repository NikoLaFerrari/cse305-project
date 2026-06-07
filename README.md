# Parallel Hierarchical Clustering

Two linkage criteria, four implementations — all benchmarked sequentially and in parallel.

## Implementations

| File | Linkage | Method |
|------|---------|--------|
| `benchmark.cpp` | Single-link | Kruskal + Borůvka |
| `benchmarkpq.cpp` | Centroid | Priority queue |
| `benchmark.cpp` | Centroid | Reverse neighbour lists |

## Build

```bash
# Single-link (Kruskal + Borůvka)
make

# Centroid — priority queue
g++ -O2 -std=c++17 -pthread benchmark_pq_final.cpp -o benchmark_pq

# Centroid — reverse neighbours
g++ -O2 -std=c++17 -pthread benchmark_rn.cpp -o benchmark_rn
```

## Run

```bash
./benchmarkpq
./benchmark
```

The dataset can be found in dataset.zip

## Output columns

`Seq` `Par-1` `Par-2` `Par-4` `Par-8` — wall-clock time in ms. `Speedup-4` — Seq / Par-4. `Match` — whether all parallel runs agree with sequential.

