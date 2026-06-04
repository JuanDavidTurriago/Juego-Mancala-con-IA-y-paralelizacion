#include "alphabeta.h"
#include "board.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Args {
    std::string algo;
    int depth = -1;
    int simulations = -1;
    std::string positions = "tests/suite.txt";
};

struct AvgStats {
    double elapsed_ms = 0.0;
    double nodes = 0.0;
    double prunes = 0.0;
};

void print_usage() {
    std::cout << "Uso:\n"
              << "  ./bench --algo alphabeta --depth N [--positions tests/suite.txt]\n"
              << "  ./bench --algo mcts --simulations N [--positions tests/suite.txt]\n";
}

bool parse_args(int argc, char** argv, Args& args) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--algo" && i + 1 < argc) {
            args.algo = argv[++i];
        } else if (arg == "--depth" && i + 1 < argc) {
            args.depth = std::stoi(argv[++i]);
        } else if (arg == "--simulations" && i + 1 < argc) {
            args.simulations = std::stoi(argv[++i]);
        } else if (arg == "--positions" && i + 1 < argc) {
            args.positions = argv[++i];
        } else {
            return false;
        }
    }

    if (args.algo == "alphabeta") {
        return args.depth > 0;
    }
    if (args.algo == "mcts") {
        return args.simulations > 0;
    }
    return false;
}

std::vector<int> parse_position_values(std::string line) {
    std::replace(line.begin(), line.end(), ',', ' ');
    std::stringstream ss(line);
    std::vector<int> values;
    int value = 0;
    while (ss >> value) {
        values.push_back(value);
    }
    return values;
}

std::vector<Board> load_positions(const std::string& path) {
    std::ifstream in(path);
    std::vector<Board> positions;
    std::string line;
    int line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;
        const auto first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || line[first] == '#') {
            continue;
        }

        const std::vector<int> values = parse_position_values(line);
        if (values.size() != 14 && values.size() != 15) {
            std::cerr << "warning: linea " << line_no << " ignorada (columnas invalidas)\n";
            continue;
        }

        Board board;
        for (int i = 0; i < kBoardSize; ++i) {
            board.pits[i] = values[i];
        }
        board.side_to_move = values.size() == 15 ? values[14] : 0;

        if (total_seed_count(board) != kInitialSeedTotal || !is_valid_side(board.side_to_move)) {
            std::cerr << "warning: linea " << line_no << " ignorada (estado invalido)\n";
            continue;
        }
        positions.push_back(board);
    }

    return positions;
}

AvgStats average_sequential(const std::vector<Board>& positions, int depth) {
    AvgStats stats;
    for (const Board& pos : positions) {
        ABResult result = best_move_alphabeta(pos, depth);
        stats.elapsed_ms += result.elapsed_ms;
        stats.nodes += result.nodes;
        stats.prunes += result.prunes;
    }

    const double count = static_cast<double>(positions.size());
    stats.elapsed_ms /= count;
    stats.nodes /= count;
    stats.prunes /= count;
    return stats;
}

AvgStats average_parallel(const std::vector<Board>& positions, int depth, int threads) {
    AvgStats stats;
    for (const Board& pos : positions) {
        ABParallelResult result = best_move_parallel(pos, depth, threads);
        stats.elapsed_ms += result.elapsed_ms;
        stats.nodes += static_cast<double>(result.nodes);
        stats.prunes += static_cast<double>(result.prunes);
    }

    const double count = static_cast<double>(positions.size());
    stats.elapsed_ms /= count;
    stats.nodes /= count;
    stats.prunes /= count;
    return stats;
}

void print_row(const std::string& label, const AvgStats& stats, double speedup, double efficiency) {
    std::cout << std::setw(7) << label << " | "
              << std::setw(7) << std::fixed << std::setprecision(1) << stats.elapsed_ms << " | "
              << std::setw(5) << std::setprecision(2) << speedup << " | "
              << std::setw(5) << std::setprecision(2) << efficiency << " | "
              << std::setw(9) << std::fixed << std::setprecision(0) << stats.nodes << " | "
              << std::setw(10) << std::fixed << std::setprecision(0) << stats.prunes << "\n";
}

int run_alphabeta(const Args& args) {
    std::vector<Board> positions = load_positions(args.positions);
    if (positions.empty()) {
        std::cerr << "No hay posiciones validas en " << args.positions << "\n";
        return 1;
    }

    std::cout << "=== Alpha-Beta Benchmark - depth=" << args.depth << " - "
              << positions.size() << " posiciones ===\n\n";

    AvgStats seq = average_sequential(positions, args.depth);

    std::cout << "[Secuencial puro]\n";
    std::cout << "threads | T(p) ms | S(p) | E(p) | nodes_avg | prunes_avg\n";
    std::cout << "--------|---------|------|------|-----------|-----------\n";
    print_row("seq", seq, 0.0, 0.0);

    std::cout << "\n[Root Parallelism]\n";
    std::cout << "threads | T(p) ms | S(p) | E(p) | nodes_avg | prunes_avg\n";
    std::cout << "--------|---------|------|------|-----------|-----------\n";

    const std::vector<int> thread_counts{1, 2, 4, 8};
    AvgStats p1 = average_parallel(positions, args.depth, 1);
    for (int p : thread_counts) {
        AvgStats stats = p == 1 ? p1 : average_parallel(positions, args.depth, p);
        double speedup = stats.elapsed_ms > 0.0 ? p1.elapsed_ms / stats.elapsed_ms : 0.0;
        double efficiency = p > 0 ? speedup / static_cast<double>(p) : 0.0;
        print_row(std::to_string(p), stats, speedup, efficiency);
    }

    std::cout << "\nNota: el paralelo con p=1 puede explorar mas nodos que el secuencial puro\n"
              << "porque cada movimiento de la raiz usa ventana independiente.\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, args)) {
        print_usage();
        return 1;
    }

    if (args.algo == "mcts") {
        std::cout << "MCTS benchmark: pendiente (P2)\n";
        return 0;
    }

    return run_alphabeta(args);
}
