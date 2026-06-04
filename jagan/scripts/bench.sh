#!/usr/bin/env bash
set -euo pipefail

DATASETS=(
  "../dataset/synthetic/2spiral.csv 2 1 1"
  "../dataset/synthetic/3MC.csv 3 1 1"
  "../dataset/synthetic/cure-t2-4k.csv 4 1 1"
  "../dataset/synthetic/banana.csv 2 1 1"
  "../dataset/synthetic/chainlink.csv 2 1 1"
)

THREADS=(1 2 4 8)

echo "dataset,k,mode,threads,edge_time,sort_time,kruskal_time,total_time,merges"

for entry in "${DATASETS[@]}"; do
    read -r path k header label <<< "$entry"
    dataset=$(basename "$path")

    # Sequential baseline
    out=$(./hac_sequential "$path" "$k" "$header" "$label" 0 seq 2>&1)
    edge=$(echo "$out" | grep "Edge computation" | awk '{print $3}')
    sort=$(echo "$out" | grep "Sorting" | awk '{print $2}')
    kruskal=$(echo "$out" | grep "Kruskal/DSU" | awk '{print $2}')
    total=$(echo "$out" | grep "Total" | awk '{print $2}')
    merges=$(echo "$out" | grep "Number of merges" | awk '{print $4}')
    echo "$dataset,$k,seq,1,$edge,$sort,$kruskal,$total,$merges"

    for t in "${THREADS[@]}"; do
        out=$(./hac_sequential "$path" "$k" "$header" "$label" 0 par "$t" 2>&1)
        edge=$(echo "$out" | grep "Edge computation" | awk '{print $3}')
        sort=$(echo "$out" | grep "Sorting" | awk '{print $2}')
        kruskal=$(echo "$out" | grep "Kruskal/DSU" | awk '{print $2}')
        total=$(echo "$out" | grep "Total" | awk '{print $2}')
        merges=$(echo "$out" | grep "Number of merges" | awk '{print $4}')
        echo "$dataset,$k,par,$t,$edge,$sort,$kruskal,$total,$merges"
    done
done
