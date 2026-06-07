#pragma once

#include "edge.hpp"
#include "merge.hpp"
#include "point.hpp"
#include "result.hpp"

#include <vector>

std::vector<Merge> kruskal_replay(
    int n,
    std::vector<Edge>& edges,
    int target_k
);

ClusteringResult kruskal_seq(
    const std::vector<Point>& points,
    int target_k
);

ClusteringResult kruskal_par(
    const std::vector<Point>& points,
    int target_k,
    int num_threads
);
