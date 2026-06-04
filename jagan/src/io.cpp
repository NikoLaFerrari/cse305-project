#include "io.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

static char detect_delimiter(const std::string& line) {
    if (line.find(',') != std::string::npos) {
        return ',';
    }

    if (line.find(';') != std::string::npos) {
        return ';';
    }

    if (line.find('\t') != std::string::npos) {
        return '\t';
    }

    return ' ';
}

static std::vector<std::string> split_line(const std::string& line, char delimiter) {
    std::vector<std::string> cells;

    if (delimiter == ' ') {
        std::stringstream ss(line);
        std::string cell;

        while (ss >> cell) {
            cells.push_back(cell);
        }

        return cells;
    }

    std::stringstream ss(line);
    std::string cell;

    while (std::getline(ss, cell, delimiter)) {
        cells.push_back(cell);
    }

    return cells;
}

static bool try_parse_double(const std::string& s, double& value) {
    try {
        std::size_t idx = 0;
        value = std::stod(s, &idx);

        while (idx < s.size()) {
            if (!std::isspace(static_cast<unsigned char>(s[idx]))) {
                return false;
            }

            idx++;
        }

        return true;
    } catch (...) {
        return false;
    }
}

std::vector<Point> read_dataset(
    const std::string& filename,
    bool has_header,
    bool last_column_is_label
) {
    std::ifstream file(filename);

    if (!file) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::vector<Point> points;
    std::string line;

    if (has_header) {
        std::getline(file, line);
    }

    bool delimiter_detected = false;
    char delimiter = ',';

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (!delimiter_detected) {
            delimiter = detect_delimiter(line);
            delimiter_detected = true;
        }

        std::vector<std::string> cells = split_line(line, delimiter);

        if (cells.empty()) {
            continue;
        }

        int usable_columns = static_cast<int>(cells.size());

        if (last_column_is_label) {
            usable_columns--;
        }

        if (usable_columns <= 0) {
            continue;
        }

        Point point;

        for (int i = 0; i < usable_columns; ++i) {
            double value = 0.0;

            if (!try_parse_double(cells[i], value)) {
                throw std::runtime_error(
                    "Non-numeric feature value found: " + cells[i]
                );
            }

            point.values.push_back(value);
        }

        points.push_back(point);
    }

    if (points.empty()) {
        throw std::runtime_error("No valid points found.");
    }

    std::size_t dim = points[0].values.size();

    for (const Point& point : points) {
        if (point.values.size() != dim) {
            throw std::runtime_error("Inconsistent point dimensions.");
        }
    }

    return points;
}

void print_merges(const std::vector<Merge>& merges) {
    std::cout << "cluster_a,cluster_b,new_cluster,edge_u,edge_v,distance,size\n";

    for (const Merge& merge : merges) {
        std::cout
            << merge.cluster_a << ","
            << merge.cluster_b << ","
            << merge.new_cluster << ","
            << merge.edge_u << ","
            << merge.edge_v << ","
            << merge.distance << ","
            << merge.size << "\n";
    }
}
