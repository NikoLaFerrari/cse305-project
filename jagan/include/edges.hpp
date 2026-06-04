#pragma once

#include "edge.hpp"
#include "point.hpp"

#include <vector>

std::vector<Edge> compute_edges_sequential(const std::vector<Point>& points);

std::vector<Edge> compute_edges_parallel(
    const std::vector<Point>& points,
    int num_threads
);
