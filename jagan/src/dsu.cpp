#include "dsu.hpp"

#include <numeric>
#include <utility>

DSU::DSU(int n)
    : parent(n), rank(n, 0), component_size(n, 1), cluster_id(n) {
    std::iota(parent.begin(), parent.end(), 0);
    std::iota(cluster_id.begin(), cluster_id.end(), 0);
}

int DSU::find(int x) {
    if (parent[x] != x) {
        parent[x] = find(parent[x]);
    }

    return parent[x];
}

int DSU::find_const(int x) const {
    while (parent[x] != x) {
        x = parent[x];
    }

    return x;
}

bool DSU::unite(
    int a,
    int b,
    int new_cluster_id,
    int& old_cluster_a,
    int& old_cluster_b,
    int& new_size
) {
    int root_a = find(a);
    int root_b = find(b);

    if (root_a == root_b) {
        return false;
    }

    old_cluster_a = cluster_id[root_a];
    old_cluster_b = cluster_id[root_b];

    if (rank[root_a] < rank[root_b]) {
        std::swap(root_a, root_b);
    }

    parent[root_b] = root_a;
    component_size[root_a] += component_size[root_b];
    cluster_id[root_a] = new_cluster_id;

    if (rank[root_a] == rank[root_b]) {
        rank[root_a]++;
    }

    new_size = component_size[root_a];

    return true;
}
