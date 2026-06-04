#include "clustering.hpp"
#include "io.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr
            << "Usage:\n"
            << "  ./hac_sequential <dataset_path> <target_k> [has_header] [last_column_is_label]\n\n"
            << "Examples:\n"
            << "  ./hac_sequential data.csv 3\n"
            << "  ./hac_sequential data.csv 3 1 1\n";

        return 1;
    }

    std::string filename = argv[1];
    int target_k = std::stoi(argv[2]);

    bool has_header = false;
    bool last_column_is_label = false;
    bool should_print_merges = false;

    if (argc >= 4) {
        has_header = std::stoi(argv[3]) != 0;
    }

    if (argc >= 5) {
        last_column_is_label = std::stoi(argv[4]) != 0;
    }

    if (argc >= 6) {
        should_print_merges = std::stoi(argv[5]) != 0;
    }
    std::string mode = "seq";
    
    int num_threads = 1;

    if (argc >= 7) {
        mode = argv[6];
    }

    if (argc >= 8) {
        num_threads = std::stoi(argv[7]);
    }

    try {
        std::vector<Point> points = read_dataset(
            filename,
            has_header,
            last_column_is_label
        );

        std::cerr << "Loaded " << points.size()
                  << " points, dimension "
                  << points[0].values.size()
                  << ".\n";


        ClusteringResult result;

        if (mode == "seq") {
            result = single_link_hac_sequential_timed(points, target_k);
        } else if (mode == "par") {
            result = single_link_hac_parallel_timed(points, target_k, num_threads);
        } else {
            throw std::runtime_error("Unknown mode. Use 'seq' or 'par'.");
        }

        std::cerr << "Mode: " << mode << "\n";
        if (mode == "par") {
            std::cerr << "Threads: " << num_threads << "\n";
        }

        std::cerr << "Timing breakdown:\n";
        std::cerr << "  Edge computation: " << result.timing.edge_time << " seconds\n";
        std::cerr << "  Sorting:          " << result.timing.sort_time << " seconds\n";
        std::cerr << "  Kruskal/DSU:      " << result.timing.kruskal_time << " seconds\n";
        std::cerr << "  Total:            " << result.timing.total_time << " seconds\n";

        std::cerr << "Number of merges: " << result.merges.size() << "\n";
         
        if(should_print_merges) print_merges(result.merges);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
