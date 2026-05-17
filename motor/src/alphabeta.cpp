#include "alphabeta.h"

#include <algorithm>
#include <limits>

AlphaBetaEngine::AlphaBetaEngine(double alpha_weight) : alpha_weight_(alpha_weight) {}

AlphaBetaResult AlphaBetaEngine::SearchBestMove(const Board& board, int depth) {
  AlphaBetaResult result;
  result.move = -1;
  result.evaluation = std::numeric_limits<int>::min();

  int side = board.side_to_move();
  auto moves = board.LegalMoves(side);
  if (moves.empty()) return result;

  int alpha = std::numeric_limits<int>::min() / 2;
  int beta = std::numeric_limits<int>::max() / 2;

  for (int move : moves) {
    Board child = board;
    child.ApplyMove(move);
    int value = -Negamax(child, depth - 1, -beta, -alpha, side, result.stats);
    if (value > result.evaluation) {
      result.evaluation = value;
      result.move = move;
    }
    alpha = std::max(alpha, value);
  }
  return result;
}

int AlphaBetaEngine::Evaluate(const Board& board, int pov_side) const {
  int me = pov_side;
  int opp = 1 - pov_side;
  int store_diff = board.Store(me) - board.Store(opp);
  int side_diff = board.SideSeeds(me) - board.SideSeeds(opp);
  double score = static_cast<double>(store_diff) + alpha_weight_ * static_cast<double>(side_diff);
  return static_cast<int>(score);
}

int AlphaBetaEngine::Negamax(Board board, int depth, int alpha, int beta, int pov_side, AlphaBetaStats& stats) {
  stats.nodes++;
  if (depth <= 0 || board.IsGameOver()) {
    return Evaluate(board, pov_side);
  }

  int side = board.side_to_move();
  auto moves = board.LegalMoves(side);
  if (moves.empty()) return Evaluate(board, pov_side);

  int best = std::numeric_limits<int>::min() / 2;
  for (int move : moves) {
    Board child = board;
    child.ApplyMove(move);
    int value = -Negamax(child, depth - 1, -beta, -alpha, pov_side, stats);
    best = std::max(best, value);
    alpha = std::max(alpha, value);
    if (alpha >= beta) {
      stats.prunes++;
      break;
    }
  }
  return best;
}

