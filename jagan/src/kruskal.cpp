#include "kruskal.hpp"
#include "distance.hpp"
#include "dsu.hpp"
#include "edges.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <thread>
#include <vector>
#include <future>
#include <utility>

static int sanitize_thread_count(int num_threads) {
    if (num_threads <= 0) {
        return 1;
    }

    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) {
        return num_threads;
    }

    return std::min(num_threads, static_cast<int>(hw));
}

static std::size_t edge_index(int i, int j, int n) {
    // Assumes 0 <= i < j < n.
    std::size_t base =
        static_cast<std::size_t>(i) *
        static_cast<std::size_t>(2 * n - i - 1) / 2;

    return base + static_cast<std::size_t>(j - i - 1);
}

static std::vector<Edge> compute_edges_par_std(
    const std::vector<Point>& points,
    int num_threads
) {
    int n = static_cast<int>(points.size());
    num_threads = sanitize_thread_count(num_threads);

    std::size_t m =
        static_cast<std::size_t>(n) * static_cast<std::size_t>(n - 1) / 2;

    std::vector<Edge> edges(m);

    // Dynamic row scheduling using an atomic counter. This balances the
    // triangular loop because early rows contain more work than later rows.
    std::atomic<int> next_i(0);
    const int chunk_size = 16;

    std::vector<std::thread> workers;
    workers.reserve(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        workers.emplace_back([&]() {
            while (true) {
                int start = next_i.fetch_add(chunk_size, std::memory_order_relaxed);

                if (start >= n) {
                    break;
                }

                int end = std::min(start + chunk_size, n);

                for (int i = start; i < end; ++i) {
                    for (int j = i + 1; j < n; ++j) {
                        std::size_t idx = edge_index(i, j, n);
                        edges[idx] = {
                            i,
                            j,
                            squared_euclidean_distance(points[i], points[j])
                        };
                    }
                }
            }
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    return edges;
}

static bool edge_less(const Edge& a, const Edge& b) {
    return a.weight < b.weight;
}

static void sort_edges_seq(std::vector<Edge>& edges) {
    std::sort(edges.begin(), edges.end(), edge_less);
}

static void sort_edges_par(std::vector<Edge>& edges, int num_threads) {
    const std::size_t n = edges.size();

    if (n <= 1) {
        return;
    }

    if (num_threads <= 1 || n < 100000) {
        sort_edges_seq(edges);
        return;
    }

    int threads = std::min<int>(num_threads, static_cast<int>(n));
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    ranges.reserve(threads);

    std::size_t block = (n + threads - 1) / threads;

    for (int t = 0; t < threads; ++t) {
        std::size_t begin = static_cast<std::size_t>(t) * block;
        std::size_t end = std::min(begin + block, n);

        if (begin < end) {
            ranges.push_back({begin, end});
        }
    }

    threads = static_cast<int>(ranges.size());

    // Step 1: sort each chunk independently.
    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (int t = 0; t < threads; ++t) {
        auto [begin, end] = ranges[t];

        workers.emplace_back([&edges, begin, end]() {
            std::sort(edges.begin() + begin, edges.begin() + end, edge_less);
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    // Step 2: merge chunks pairwise.
    std::vector<Edge> buffer(n);

    while (ranges.size() > 1) {
        std::vector<std::pair<std::size_t, std::size_t>> new_ranges;
        new_ranges.reserve((ranges.size() + 1) / 2);

        workers.clear();

        for (std::size_t r = 0; r + 1 < ranges.size(); r += 2) {
            std::size_t begin1 = ranges[r].first;
            std::size_t end1 = ranges[r].second;
            std::size_t begin2 = ranges[r + 1].first;
            std::size_t end2 = ranges[r + 1].second;

            workers.emplace_back([&, begin1, end1, begin2, end2]() {
                std::merge(
                    edges.begin() + begin1,
                    edges.begin() + end1,
                    edges.begin() + begin2,
                    edges.begin() + end2,
                    buffer.begin() + begin1,
                    edge_less
                );
            });

            new_ranges.push_back({begin1, end2});
        }

        // Odd leftover range: copy it directly.
        if (ranges.size() % 2 == 1) {
            std::size_t begin = ranges.back().first;
            std::size_t end = ranges.back().second;

            workers.emplace_back([&, begin, end]() {
                std::copy(
                    edges.begin() + begin,
                    edges.begin() + end,
                    buffer.begin() + begin
                );
            });

            new_ranges.push_back({begin, end});
        }

        for (std::thread& worker : workers) {
            worker.join();
        }

        // Copy merged result back to edges.
        workers.clear();

        for (const auto& range : new_ranges) {
            std::size_t begin = range.first;
            std::size_t end = range.second;

            workers.emplace_back([&, begin, end]() {
                std::copy(
                    buffer.begin() + begin,
                    buffer.begin() + end,
                    edges.begin() + begin
                );
            });
        }

        for (std::thread& worker : workers) {
            worker.join();
        }

        ranges = std::move(new_ranges);
    }
}

std::vector<Merge> kruskal_replay(
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

    sort_edges_seq(edges);

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
                std::sqrt(edge.weight),
                new_size
            });

            next_cluster_id++;
            current_clusters--;
        }
    }

    return merges;
}

ClusteringResult kruskal_seq(
    const std::vector<Point>& points,
    int target_k
) {
    using clock = std::chrono::high_resolution_clock;

    auto total_start = clock::now();

    auto edge_start = clock::now();
    std::vector<Edge> edges = compute_edges_seq(points);
    auto edge_end = clock::now();

    auto sort_start = clock::now();
    sort_edges_seq(edges);
    auto sort_end = clock::now();

    auto mst_start = clock::now();
    DSU dsu(static_cast<int>(points.size()));
    std::vector<Merge> merges;
    merges.reserve(points.size() - target_k);

    int current_clusters = static_cast<int>(points.size());
    int next_cluster_id = static_cast<int>(points.size());

    for (const Edge& edge : edges) {
        if (current_clusters == target_k) {
            break;
        }

        int old_a = -1;
        int old_b = -1;
        int new_size = 0;

        if (dsu.unite(edge.u, edge.v, next_cluster_id, old_a, old_b, new_size)) {
            merges.push_back({
                old_a,
                old_b,
                next_cluster_id,
                edge.u,
                edge.v,
                std::sqrt(edge.weight),
                new_size
            });

            next_cluster_id++;
            current_clusters--;
        }
    }

    auto mst_end = clock::now();
    auto total_end = clock::now();

    TimingInfo timing;
    timing.edge_time = std::chrono::duration<double>(edge_end - edge_start).count();
    timing.sort_time = std::chrono::duration<double>(sort_end - sort_start).count();
    timing.mst_time = std::chrono::duration<double>(mst_end - mst_start).count();
    timing.total_time = std::chrono::duration<double>(total_end - total_start).count();

    return {merges, timing};
}

ClusteringResult kruskal_par(
    const std::vector<Point>& points,
    int target_k,
    int num_threads
) {
    using clock = std::chrono::high_resolution_clock;

    auto total_start = clock::now();

    auto edge_start = clock::now();
    std::vector<Edge> edges = compute_edges_par_std(points, num_threads);
    auto edge_end = clock::now();

    auto sort_start = clock::now();
    sort_edges_par(edges, num_threads);
    auto sort_end = clock::now();

    auto mst_start = clock::now();
    DSU dsu(static_cast<int>(points.size()));
    std::vector<Merge> merges;
    merges.reserve(points.size() - target_k);

    int current_clusters = static_cast<int>(points.size());
    int next_cluster_id = static_cast<int>(points.size());

    for (const Edge& edge : edges) {
        if (current_clusters == target_k) {
            break;
        }

        int old_a = -1;
        int old_b = -1;
        int new_size = 0;

        if (dsu.unite(edge.u, edge.v, next_cluster_id, old_a, old_b, new_size)) {
            merges.push_back({
                old_a,
                old_b,
                next_cluster_id,
                edge.u,
                edge.v,
                std::sqrt(edge.weight),
                new_size
            });

            next_cluster_id++;
            current_clusters--;
        }
    }

    auto mst_end = clock::now();
    auto total_end = clock::now();

    TimingInfo timing;
    timing.edge_time = std::chrono::duration<double>(edge_end - edge_start).count();
    timing.sort_time = std::chrono::duration<double>(sort_end - sort_start).count();
    timing.mst_time = std::chrono::duration<double>(mst_end - mst_start).count();
    timing.total_time = std::chrono::duration<double>(total_end - total_start).count();

    return {merges, timing};
}
