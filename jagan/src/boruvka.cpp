#include "boruvka.hpp"

#include "distance.hpp"
#include "dsu.hpp"
#include "edge.hpp"
#include "kruskal.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

struct CheapestEdge {
    bool valid = false;
    int u = -1;
    int v = -1;
    double weight = std::numeric_limits<double>::infinity();
};

static void relax_edge(CheapestEdge& current, int u, int v, double weight) {
    if (!current.valid || weight < current.weight) {
        current.valid = true;
        current.u = u;
        current.v = v;
        current.weight = weight;
    }
}

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

static std::vector<Edge> boruvka_mst_seq(const std::vector<Point>& points) {
    int n = static_cast<int>(points.size());

    if (n == 0) {
        throw std::runtime_error("Empty dataset.");
    }

    DSU dsu(n);
    std::vector<Edge> mst_edges;
    mst_edges.reserve(n - 1);

    int components = n;

    while (components > 1) {
        std::vector<CheapestEdge> cheapest(n);

        for (int i = 0; i < n; ++i) {
            int ci = dsu.find(i);

            for (int j = i + 1; j < n; ++j) {
                int cj = dsu.find(j);

                if (ci == cj) {
                    continue;
                }

                double w = squared_euclidean_distance(points[i], points[j]);

                relax_edge(cheapest[ci], i, j, w);
                relax_edge(cheapest[cj], i, j, w);
            }
        }

        int merges_this_round = 0;

        for (int c = 0; c < n; ++c) {
            if (!cheapest[c].valid) {
                continue;
            }

            const CheapestEdge& e = cheapest[c];

            int old_a = -1;
            int old_b = -1;
            int new_size = 0;

            if (dsu.unite(e.u, e.v, 0, old_a, old_b, new_size)) {
                mst_edges.push_back({e.u, e.v, e.weight});
                components--;
                merges_this_round++;
            }
        }

        if (merges_this_round == 0) {
            throw std::runtime_error("Boruvka made no progress.");
        }
    }

    return mst_edges;
}

static std::vector<Edge> boruvka_mst_par(
    const std::vector<Point>& points,
    int num_threads
) {
    int n = static_cast<int>(points.size());

    if (n == 0) {
        throw std::runtime_error("Empty dataset.");
    }

    num_threads = sanitize_thread_count(num_threads);

    DSU dsu(n);
    std::vector<Edge> mst_edges;
    mst_edges.reserve(n - 1);

    int components = n;

    while (components > 1) {
        std::vector<int> root_of(n);

        // Snapshot the DSU roots. We use find_const so there is no concurrent
        // path compression and therefore no race on the DSU arrays.
        {
            std::atomic<int> next_i(0);
            const int chunk_size = 256;
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
                            root_of[i] = dsu.find_const(i);
                        }
                    }
                });
            }

            for (std::thread& worker : workers) {
                worker.join();
            }
        }

        std::vector<std::vector<CheapestEdge>> local_cheapest(
            num_threads,
            std::vector<CheapestEdge>(n)
        );

        // Parallel all-pairs scan. Work is distributed dynamically with an
        // atomic row counter. Each thread writes only to its own local array,
        // so no mutex is needed during the expensive O(n^2) phase.
        {
            std::atomic<int> next_i(0);
            const int chunk_size = 16;
            std::vector<std::thread> workers;
            workers.reserve(num_threads);

            for (int t = 0; t < num_threads; ++t) {
                workers.emplace_back([&, t]() {
                    std::vector<CheapestEdge>& local = local_cheapest[t];

                    while (true) {
                        int start = next_i.fetch_add(chunk_size, std::memory_order_relaxed);

                        if (start >= n) {
                            break;
                        }

                        int end = std::min(start + chunk_size, n);

                        for (int i = start; i < end; ++i) {
                            int ci = root_of[i];

                            for (int j = i + 1; j < n; ++j) {
                                int cj = root_of[j];

                                if (ci == cj) {
                                    continue;
                                }

                                double w = squared_euclidean_distance(points[i], points[j]);

                                relax_edge(local[ci], i, j, w);
                                relax_edge(local[cj], i, j, w);
                            }
                        }
                    }
                });
            }

            for (std::thread& worker : workers) {
                worker.join();
            }
        }

        std::vector<CheapestEdge> cheapest(n);

        // Sequential reduction of thread-local cheapest edges.
        for (int t = 0; t < num_threads; ++t) {
            for (int c = 0; c < n; ++c) {
                if (local_cheapest[t][c].valid) {
                    relax_edge(
                        cheapest[c],
                        local_cheapest[t][c].u,
                        local_cheapest[t][c].v,
                        local_cheapest[t][c].weight
                    );
                }
            }
        }

        int merges_this_round = 0;

        // Merge phase kept sequential to avoid concurrent DSU races.
        for (int c = 0; c < n; ++c) {
            if (!cheapest[c].valid) {
                continue;
            }

            const CheapestEdge& e = cheapest[c];

            int old_a = -1;
            int old_b = -1;
            int new_size = 0;

            if (dsu.unite(e.u, e.v, 0, old_a, old_b, new_size)) {
                mst_edges.push_back({e.u, e.v, e.weight});
                components--;
                merges_this_round++;
            }
        }

        if (merges_this_round == 0) {
            throw std::runtime_error("Boruvka made no progress.");
        }
    }

    return mst_edges;
}

ClusteringResult boruvka_seq(
    const std::vector<Point>& points,
    int target_k
) {
    using clock = std::chrono::high_resolution_clock;

    auto total_start = clock::now();

    auto mst_start = clock::now();
    std::vector<Edge> mst_edges = boruvka_mst_seq(points);
    auto mst_end = clock::now();

    auto replay_start = clock::now();
    std::vector<Merge> merges = kruskal_replay(
        static_cast<int>(points.size()),
        mst_edges,
        target_k
    );
    auto replay_end = clock::now();

    auto total_end = clock::now();

    TimingInfo timing;
    timing.mst_time = std::chrono::duration<double>(mst_end - mst_start).count();
    timing.replay_time = std::chrono::duration<double>(replay_end - replay_start).count();
    timing.total_time = std::chrono::duration<double>(total_end - total_start).count();

    return {merges, timing};
}

ClusteringResult boruvka_par(
    const std::vector<Point>& points,
    int target_k,
    int num_threads
) {
    using clock = std::chrono::high_resolution_clock;

    auto total_start = clock::now();

    auto mst_start = clock::now();
    std::vector<Edge> mst_edges = boruvka_mst_par(points, num_threads);
    auto mst_end = clock::now();

    auto replay_start = clock::now();
    std::vector<Merge> merges = kruskal_replay(
        static_cast<int>(points.size()),
        mst_edges,
        target_k
    );
    auto replay_end = clock::now();

    auto total_end = clock::now();

    TimingInfo timing;
    timing.mst_time = std::chrono::duration<double>(mst_end - mst_start).count();
    timing.replay_time = std::chrono::duration<double>(replay_end - replay_start).count();
    timing.total_time = std::chrono::duration<double>(total_end - total_start).count();

    return {merges, timing};
}
