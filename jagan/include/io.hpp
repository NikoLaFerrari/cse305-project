#pragma once

#include "merge.hpp"
#include "point.hpp"

#include <string>
#include <vector>

std::vector<Point> read_dataset(
    const std::string& filename,
    bool has_header,
    bool last_column_is_label
);

void print_merges(const std::vector<Merge>& merges);
