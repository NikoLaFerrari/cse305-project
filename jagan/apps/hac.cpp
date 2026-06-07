#include "boruvka.hpp"
#include "io.hpp"
#include "kruskal.hpp"
#include "result.hpp"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr
            << "Usage:\n"
            << "  ./hac <dataset> <target_k> <algorithm> <mode> <threads> "
            << "[has_header] [last_column_is_label] [print_merges]\n\n"
            << "Algorithms:\n"
            << "  kruskal\n"
            << "  boruvka\n\n"
            << "Modes:\n"
            << "  seq\n"
            << "  par\n\n"
            << "Examples:\n"
            << "  ./hac data.csv 3 kruskal seq 1\n"
            << "  ./hac data.csv 3 kruskal par 8\n"
            << "  ./hac data.csv 3 boruvka seq 1\n"
            << "  ./hac data.csv 3 boruvka par 8\n";

        return 1;
    }

    std::string filename = argv[1];
    int target_k = std::stoi(argv[2]);
    std::string algorithm = argv[3];
    std::string mode = argv[4];
    int threads = std::stoi(argv[5]);

    bool has_header = false;
    bool last_column_is_label = false;
    bool should_print_merges = false;

    if (argc >= 7) {
        has_header = std::stoi(argv[6]) != 0;
    }

    if (argc >= 8) {
        last_column_is_label = std::stoi(argv[7]) != 0;
    }

    if (argc >= 9) {
        should_print_merges = std::stoi(argv[8]) != 0;
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

        if (algorithm == "kruskal" && mode == "seq") {
            result = kruskal_seq(points, target_k);
        } else if (algorithm == "kruskal" && mode == "par") {
            result = kruskal_par(points, target_k, threads);
        } else if (algorithm == "boruvka" && mode == "seq") {
            result = boruvka_seq(points, target_k);
        } else if (algorithm == "boruvka" && mode == "par") {
            result = boruvka_par(points, target_k, threads);
        } else {
            throw std::runtime_error("Unknown algorithm/mode combination.");
        }

        std::cerr << "Algorithm: " << algorithm << "\n";
        std::cerr << "Mode:      " << mode << "\n";
        std::cerr << "Threads:   " << threads << "\n";

        std::cerr << "Timing breakdown:\n";
        std::cerr << "  Edge computation: " << result.timing.edge_time << " seconds\n";
        std::cerr << "  Sorting:          " << result.timing.sort_time << " seconds\n";
        std::cerr << "  MST/Boruvka:      " << result.timing.mst_time << " seconds\n";
        std::cerr << "  Replay:           " << result.timing.replay_time << " seconds\n";
        std::cerr << "  Total:            " << result.timing.total_time << " seconds\n";

        std::cerr << "Number of merges: " << result.merges.size() << "\n";

        if (should_print_merges) {
            print_merges(result.merges);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
