#pragma once

#include "edge.hpp"
#include "merge.hpp"
#include "point.hpp"

#include <vector>

struct TimingInfo {
    double edge_time;
    double sort_time;
    double kruskal_time;
    double total_time;
};

struct ClusteringResult {
    std::vector<Merge> merges;
    TimingInfo timing;
};

std::vector<Merge> kruskal_merges(
    int n,
    std::vector<Edge>& edges,
    int target_k
);

ClusteringResult single_link_hac_sequential_timed(
    const std::vector<Point>& points,
    int target_k
);

ClusteringResult single_link_hac_parallel_timed(
    const std::vector<Point>& points,
    int target_k,
    int num_threads
);

std::vector<Merge> single_link_hac_sequential(
    const std::vector<Point>& points,
    int target_k
);
