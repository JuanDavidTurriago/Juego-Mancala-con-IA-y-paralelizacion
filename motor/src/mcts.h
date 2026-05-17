#pragma once

#include "board.h"

#include <cstdint>

struct MctsStats {
  std::int64_t rollouts = 0;
  double win_rate = 0.0;
  double tree_depth_avg = 0.0;
};

struct MctsResult {
  int move = -1;          // 0..5 (relative)
  double evaluation = 0;  // win_rate estimate [0..1]
  MctsStats stats{};
};

class MctsEngine {
 public:
  explicit MctsEngine(double exploration_c = 1.41421356237);

  MctsResult SearchBestMove(const Board& board, int simulations);

 private:
  double exploration_c_ = 1.41421356237;
};

