# Parallel Hierarchical Clustering

Two linkage criteria, four implementations — all benchmarked sequentially and in parallel.

## Implementations

| File | Linkage | Method |
|------|---------|--------|
| `src/kruskal.cpp` | Single-link# Parallel Hierarchical Clustering

Two linkage criteria, four implementations — all benchmarked sequentially and in parallel.

## Implementations

| File | Linkage | Method |
|------|---------|--------|
| `src/kruskal.cpp` | Single-link | Kruskal |
| `src/boruvka.cpp` | Single-link | Borůvka |
| `benchmarkpq.cpp` | Centroid | Priority queue |
| `benchmark.cpp` | Centroid | Reverse neighbour lists |

## Build

```bash
# Single-link (Kruskal + Borůvka)
cd jagan
make

# Centroid — priority queue
cd krzysztof
g++ -O2 -std=c++17 -pthread benchmarkpq_final.cpp -o benchmarkpq

# Centroid — reverse neighbours
g++ -O2 -std=c++17 -pthread benchmark.cpp -o benchmark
```

## Run

```bash
Single-link manual run

# Command format:

./hac <dataset> <target_k> <algorithm> <mode> <threads> [has_header] [last_column_is_label] [print_merges]

# For files without a label column, use:

./hac ../dataset/processed/birch-rg1-5k.csv 100 boruvka par 8 1 0 0

# Single-link automated benchmark

# From jagan/:

./scripts/run_all_benchmarks.py \
  --dataset-root ../dataset \
  --hac ./hac \
  --threads 1,2,4,8 \
  --max-n 100000 \
  --timeout 1000 \
  --include-arff

# Outputs:
results/benchmark_results.csv
results/benchmark_log.txt


# Centroid
./benchmarkpq
./benchmark
```

The dataset can be found in dataset.zip

## Output columns

`Seq` `Par-1` `Par-2` `Par-4` `Par-8` — wall-clock time in ms. `Speedup-4` — Seq / Par-4. `Match` — whether all parallel runs agree with sequential.
