#include "edges.hpp"

#include "distance.hpp"

#include <cstddef>

std::vector<Edge> compute_edges_sequential(const std::vector<Point>& points) {
    int n = static_cast<int>(points.size());

    std::vector<Edge> edges;
    edges.reserve(static_cast<std::size_t>(n) * static_cast<std::size_t>(n - 1) / 2);

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            edges.push_back({i, j, euclidean_distance(points[i], points[j])});
        }
    }

    return edges;
}

std::vector<Edge> compute_edges_parallel(
    const std::vector<Point>& points,
    int num_threads
) {
    int n = static_cast<int>(points.size());
    std::size_t m =
        static_cast<std::size_t>(n) * static_cast<std::size_t>(n - 1) / 2;

    std::vector<Edge> edges(m);

    #pragma omp parallel for num_threads(num_threads) schedule(static)
    for (int i = 0; i < n; ++i) {
        std::size_t base =
            static_cast<std::size_t>(i) *
            static_cast<std::size_t>(2 * n - i - 1) / 2;

        for (int j = i + 1; j < n; ++j) {
            std::size_t idx = base + static_cast<std::size_t>(j - i - 1);
            edges[idx] = {i, j, euclidean_distance(points[i], points[j])};
        }
    }

    return edges;
}
