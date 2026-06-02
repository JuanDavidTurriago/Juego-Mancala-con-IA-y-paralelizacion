#include "alphabeta.h"
#include "board.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<int> parse_csv_ints(const std::string& line) {
  std::vector<int> values;
  std::stringstream ss(line);
  std::string item;
  while (std::getline(ss, item, ',')) {
    values.push_back(std::stoi(item));
  }
  return values;
}

std::vector<Board> load_positions(const std::string& path) {
  std::ifstream in(path);
  std::vector<Board> positions;
  std::string line;
  while (std::getline(in, line)) {
    const auto first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || line[first] == '#') {
      continue;
    }
    const auto values = parse_csv_ints(line);
    if (values.size() != kBoardSize) {
      continue;
    }
    Board board;
    for (int i = 0; i < kBoardSize; ++i) {
      board.pits[i] = values[i];
    }
    board.side_to_move = 0;
    if (total_seed_count(board) == kInitialSeedTotal) {
      positions.push_back(board);
    }
  }
  if (positions.empty()) {
    positions.push_back(Board{});
  }
  return positions;
}

std::vector<int> parse_threads(const std::string& text) {
  std::vector<int> threads;
  std::stringstream ss(text);
  std::string item;
  while (std::getline(ss, item, ',')) {
    threads.push_back(std::stoi(item));
  }
  return threads.empty() ? std::vector<int>{1, 2, 4, 8} : threads;
}

}  // namespace

int main(int argc, char** argv) {
  int depth = 8;
  std::string positions_path = "tests/suite.txt";
  std::vector<int> thread_counts{1, 2, 4, 8};

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--depth" && i + 1 < argc) {
      depth = std::stoi(argv[++i]);
    } else if (arg == "--positions" && i + 1 < argc) {
      positions_path = argv[++i];
    } else if (arg == "--threads" && i + 1 < argc) {
      thread_counts = parse_threads(argv[++i]);
    }
  }

  const auto positions = load_positions(positions_path);
  AlphaBetaEngine engine;

  std::cout << "position,depth,threads,move,evaluation,elapsed_ms,speedup,efficiency,nodes,prunes\n";
  for (size_t pos = 0; pos < positions.size(); ++pos) {
    double baseline_ms = 0.0;
    int baseline_move = -1;

    for (int threads : thread_counts) {
      const auto started = std::chrono::steady_clock::now();
      const AlphaBetaResult result = engine.SearchBestMove(positions[pos], depth, threads);
      const auto ended = std::chrono::steady_clock::now();
      const double elapsed_ms =
          std::chrono::duration<double, std::milli>(ended - started).count();

      if (threads == 1 || baseline_ms == 0.0) {
        baseline_ms = elapsed_ms;
        baseline_move = result.move;
      }

      const double speedup = elapsed_ms > 0.0 ? baseline_ms / elapsed_ms : 0.0;
      const double efficiency = threads > 0 ? speedup / static_cast<double>(threads) : 0.0;
      std::cout << pos << ',' << depth << ',' << threads << ',' << result.move << ','
                << result.evaluation << ',' << std::fixed << std::setprecision(3) << elapsed_ms
                << ',' << speedup << ',' << efficiency << ',' << result.stats.nodes << ','
                << result.stats.prunes;
      if (baseline_move != result.move) {
        std::cout << ",warning=different_move";
      }
      std::cout << '\n';
    }
  }
  return 0;
}
