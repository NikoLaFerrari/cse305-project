#pragma once

#include "merge.hpp"
#include <vector>

struct TimingInfo {
    double edge_time = 0.0;
    double sort_time = 0.0;
    double mst_time = 0.0;
    double replay_time = 0.0;
    double total_time = 0.0;
};

struct ClusteringResult {
    std::vector<Merge> merges;
    TimingInfo timing;
};
