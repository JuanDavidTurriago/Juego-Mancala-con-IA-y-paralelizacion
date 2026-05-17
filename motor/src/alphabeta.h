#pragma once

#include "board.h"

#include <cstdint>

struct AlphaBetaStats {
  std::int64_t nodes = 0;
  std::int64_t prunes = 0;
};

struct AlphaBetaResult {
  int move = -1;          // 0..5 (relative)
  int evaluation = 0;     // heuristic score from current player POV
  AlphaBetaStats stats{};
};

class AlphaBetaEngine {
 public:
  explicit AlphaBetaEngine(double alpha_weight = 0.5);

  AlphaBetaResult SearchBestMove(const Board& board, int depth);

 private:
  double alpha_weight_ = 0.5;

  int Evaluate(const Board& board, int pov_side) const;
  int Negamax(Board board, int depth, int alpha, int beta, int pov_side, AlphaBetaStats& stats);
};

