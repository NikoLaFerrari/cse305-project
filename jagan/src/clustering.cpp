#include "clustering.hpp"

#include "dsu.hpp"
#include "edges.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>

#ifdef USE_PARALLEL_SORT
#include <parallel/algorithm>
#endif

std::vector<Merge> kruskal_merges(
    int n,
    std::vector<Edge>& edges,
    int target_k
) {
    if (n == 0) {
        throw std::runtime_error("Empty dataset.");
    }

    if (target_k < 1 || target_k > n) {
        throw std::runtime_error("target_k must satisfy 1 <= target_k <= n.");
    }

    DSU dsu(n);
    std::vector<Merge> merges;
    merges.reserve(n - target_k);

    int current_clusters = n;
    int next_cluster_id = n;

    for (const Edge& edge : edges) {
        if (current_clusters == target_k) {
            break;
        }

        int old_cluster_a = -1;
        int old_cluster_b = -1;
        int new_size = 0;

        bool merged = dsu.unite(
            edge.u,
            edge.v,
            next_cluster_id,
            old_cluster_a,
            old_cluster_b,
            new_size
        );

        if (merged) {
            merges.push_back({
                old_cluster_a,
                old_cluster_b,
                next_cluster_id,
                edge.u,
                edge.v,
                edge.weight,
                new_size
            });

            next_cluster_id++;
            current_clusters--;
        }
    }

    return merges;
}

ClusteringResult single_link_hac_sequential_timed(
    const std::vector<Point>& points,
    int target_k
) {
    using clock = std::chrono::high_resolution_clock;

    auto total_start = clock::now();

    auto edge_start = clock::now();
    std::vector<Edge> edges = compute_edges_sequential(points);
    auto edge_end = clock::now();

    auto sort_start = clock::now();

    auto edge_cmp = [](const Edge& a, const Edge& b) {
    return a.weight < b.weight;
    };

    #ifdef USE_PARALLEL_SORT
    __gnu_parallel::sort(edges.begin(), edges.end(), edge_cmp);
    #else
    std::sort(edges.begin(), edges.end(), edge_cmp);
    #endif

    auto sort_end = clock::now();

    auto kruskal_start = clock::now();
    std::vector<Merge> merges = kruskal_merges(
        static_cast<int>(points.size()),
        edges,
        target_k
    );
    auto kruskal_end = clock::now();

    auto total_end = clock::now();

    TimingInfo timing {
        std::chrono::duration<double>(edge_end - edge_start).count(),
        std::chrono::duration<double>(sort_end - sort_start).count(),
        std::chrono::duration<double>(kruskal_end - kruskal_start).count(),
        std::chrono::duration<double>(total_end - total_start).count()
    };

    return {merges, timing};
}

std::vector<Merge> single_link_hac_sequential(
    const std::vector<Point>& points,
    int target_k
) {
    return single_link_hac_sequential_timed(points, target_k).merges;
}


ClusteringResult single_link_hac_parallel_timed(
    const std::vector<Point>& points,
    int target_k,
    int num_threads
) {
    using clock = std::chrono::high_resolution_clock;

    auto total_start = clock::now();

    auto edge_start = clock::now();
    std::vector<Edge> edges = compute_edges_parallel(points, num_threads);
    auto edge_end = clock::now();

    auto sort_start = clock::now();
    
    auto edge_cmp = [](const Edge& a, const Edge& b) {
    return a.weight < b.weight;
    };

    #ifdef USE_PARALLEL_SORT
    __gnu_parallel::sort(edges.begin(), edges.end(), edge_cmp);
    #else
    std::sort(edges.begin(), edges.end(), edge_cmp);
    #endif

    auto sort_end = clock::now();

    auto kruskal_start = clock::now();
    std::vector<Merge> merges = kruskal_merges(
        static_cast<int>(points.size()),
        edges,
        target_k
    );
    auto kruskal_end = clock::now();

    auto total_end = clock::now();

    TimingInfo timing {
        std::chrono::duration<double>(edge_end - edge_start).count(),
        std::chrono::duration<double>(sort_end - sort_start).count(),
        std::chrono::duration<double>(kruskal_end - kruskal_start).count(),
        std::chrono::duration<double>(total_end - total_start).count()
    };

    return {merges, timing};
}
