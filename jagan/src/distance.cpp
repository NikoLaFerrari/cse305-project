#include "distance.hpp"

#include <cmath>
#include <stdexcept>

double squared_euclidean_distance(const Point& a, const Point& b) {
    if (a.values.size() != b.values.size()) {
        throw std::runtime_error("Dimension mismatch between points.");
    }

    double sum = 0.0;

    for (std::size_t i = 0; i < a.values.size(); ++i) {
        double diff = a.values[i] - b.values[i];
        sum += diff * diff;
    }

    return sum;
}

double euclidean_distance(const Point& a, const Point& b) {
    return std::sqrt(squared_euclidean_distance(a, b));
}
