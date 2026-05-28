#include "alphabeta.h"

#include <algorithm>
#include <climits>
#include <limits>

AlphaBetaEngine::AlphaBetaEngine(double alpha_weight) : alpha_weight_(alpha_weight) {}

AlphaBetaResult AlphaBetaEngine::SearchBestMove(const Board& board, int depth) {
  AlphaBetaResult result;
  result.move = -1;
  result.evaluation = std::numeric_limits<int>::min();

  int side = board.side_to_move;
  auto moves = legal_moves(board);
  if (moves.empty()) return result;

  int alpha = std::numeric_limits<int>::min() / 2;
  int beta = std::numeric_limits<int>::max() / 2;

  for (int move : moves) {
    bool extra_turn = false;
    Board child = apply_move(board, move, extra_turn);
    int value = Negamax(child, depth - 1, alpha, beta, side, result.stats);
    if (value > result.evaluation) {
      result.evaluation = value;
      result.move = move;
    }
    alpha = std::max(alpha, value);
  }
  return result;
}

int AlphaBetaEngine::Evaluate(const Board& board, int pov_side) const {
  int score = evaluate(board, static_cast<float>(alpha_weight_));
  if (pov_side == 0) {
    return score;
  }
  if (score == INT_MAX) {
    return INT_MIN;
  }
  if (score == INT_MIN) {
    return INT_MAX;
  }
  return -score;
}

int AlphaBetaEngine::Negamax(Board board, int depth, int alpha, int beta, int pov_side, AlphaBetaStats& stats) {
  stats.nodes++;
  if (depth <= 0 || is_terminal(board)) {
    return Evaluate(board, pov_side);
  }

  auto moves = legal_moves(board);
  if (moves.empty()) return Evaluate(board, pov_side);

  bool maximizing = board.side_to_move == pov_side;
  int best = maximizing ? std::numeric_limits<int>::min() / 2 : std::numeric_limits<int>::max() / 2;
  for (int move : moves) {
    bool extra_turn = false;
    Board child = apply_move(board, move, extra_turn);
    int value = Negamax(child, depth - 1, alpha, beta, pov_side, stats);
    if (maximizing) {
      best = std::max(best, value);
      alpha = std::max(alpha, value);
    } else {
      best = std::min(best, value);
      beta = std::min(beta, value);
    }
    if (alpha >= beta) {
      stats.prunes++;
      break;
    }
  }
  return best;
}
