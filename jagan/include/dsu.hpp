#pragma once

#include <vector>

class DSU {
private:
    std::vector<int> parent;
    std::vector<int> rank;
    std::vector<int> component_size;
    std::vector<int> cluster_id;

public:
    explicit DSU(int n);

    int find(int x);

    bool unite(
        int a,
        int b,
        int new_cluster_id,
        int& old_cluster_a,
        int& old_cluster_b,
        int& new_size
    );
};
