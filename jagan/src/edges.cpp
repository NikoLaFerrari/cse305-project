#include "edges.hpp"

#include "distance.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

std::vector<Edge> compute_edges_seq(const std::vector<Point>& points) {
    int n = static_cast<int>(points.size());

    std::vector<Edge> edges;
    edges.reserve(static_cast<std::size_t>(n) * static_cast<std::size_t>(n - 1) / 2);

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            edges.push_back({i, j, squared_euclidean_distance(points[i], points[j])});
        }
    }

    return edges;
}

std::vector<Edge> compute_edges_par(
    const std::vector<Point>& points,
    int num_threads
) {
    int n = static_cast<int>(points.size());

    std::size_t m =
        static_cast<std::size_t>(n) * static_cast<std::size_t>(n - 1) / 2;

    std::vector<Edge> edges(m);

    if (num_threads <= 1 || n < 2) {
        return compute_edges_seq(points);
    }

    std::atomic<int> next_i(0);
    const int chunk_size = 16;

    std::vector<std::thread> workers;
    workers.reserve(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        workers.emplace_back([&, t]() {
            (void)t;

            while (true) {
                int start = next_i.fetch_add(chunk_size);

                if (start >= n) {
                    break;
                }

                int end = std::min(start + chunk_size, n);

                for (int i = start; i < end; ++i) {
                    std::size_t base =
                        static_cast<std::size_t>(i) *
                        static_cast<std::size_t>(2 * n - i - 1) / 2;

                    for (int j = i + 1; j < n; ++j) {
                        std::size_t idx =
                            base + static_cast<std::size_t>(j - i - 1);

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
