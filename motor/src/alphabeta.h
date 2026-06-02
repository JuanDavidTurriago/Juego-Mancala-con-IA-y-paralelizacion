#pragma once

#include "board.h"

#include <cstdint>

struct AlphaBetaStats {
  std::int64_t nodes = 0;
  std::int64_t prunes = 0;
};

struct AlphaBetaResult {
  int move = -1;
  int evaluation = 0;
  AlphaBetaStats stats{};
  int threads_used = 1;
};

class AlphaBetaEngine {
 public:
  explicit AlphaBetaEngine(double pit_weight = 0.5);

  AlphaBetaResult SearchBestMove(const Board& board, int depth, int threads = 1);

 private:
  double pit_weight_ = 0.5;

  int Evaluate(const Board& board, int pov_side) const;
  int Search(Board board, int depth, int alpha, int beta, int pov_side,
             AlphaBetaStats& stats) const;
};
