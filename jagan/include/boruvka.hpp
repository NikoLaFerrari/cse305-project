#pragma once

#include "point.hpp"
#include "result.hpp"

ClusteringResult boruvka_seq(
    const std::vector<Point>& points,
    int target_k
);

ClusteringResult boruvka_par(
    const std::vector<Point>& points,
    int target_k,
    int num_threads
);
