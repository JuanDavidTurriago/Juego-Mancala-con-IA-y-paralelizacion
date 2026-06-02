#include "alphabeta.h"

#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

constexpr int kNegativeInfinity = -1000000000;
constexpr int kPositiveInfinity = 1000000000;

bool better_result(int candidate_value, int candidate_move, int best_value, int best_move) {
  if (candidate_value != best_value) {
    return candidate_value > best_value;
  }
  return best_move < 0 || candidate_move < best_move;
}

}  // namespace

AlphaBetaEngine::AlphaBetaEngine(double pit_weight) : pit_weight_(pit_weight) {}

AlphaBetaResult AlphaBetaEngine::SearchBestMove(const Board& board, int depth, int threads) {
  AlphaBetaResult result;
  result.evaluation = kNegativeInfinity;
  result.stats.nodes = 1;

  const auto moves = legal_moves(board);
  if (moves.empty() || depth <= 0) {
    result.evaluation = Evaluate(board, board.side_to_move);
    return result;
  }

  const int pov_side = board.side_to_move;
  const int requested_threads = std::max(1, threads);
  std::int64_t total_nodes = 0;
  std::int64_t total_prunes = 0;
  int best_move = -1;
  int best_value = kNegativeInfinity;
  int threads_used = 1;

#ifdef _OPENMP
#pragma omp parallel num_threads(requested_threads) reduction(+ : total_nodes, total_prunes)
  {
#pragma omp single
    { threads_used = omp_get_num_threads(); }

#pragma omp for schedule(dynamic)
    for (int i = 0; i < static_cast<int>(moves.size()); ++i) {
      AlphaBetaStats local_stats;
      bool extra_turn = false;
      Board child = apply_move(board, moves[i], extra_turn);
      const int value =
          Search(child, depth - 1, kNegativeInfinity, kPositiveInfinity, pov_side, local_stats);
      total_nodes += local_stats.nodes;
      total_prunes += local_stats.prunes;

#pragma omp critical
      {
        if (better_result(value, moves[i], best_value, best_move)) {
          best_value = value;
          best_move = moves[i];
        }
      }
    }
  }
#else
  (void)requested_threads;
  for (int move : moves) {
    AlphaBetaStats local_stats;
    bool extra_turn = false;
    Board child = apply_move(board, move, extra_turn);
    const int value =
        Search(child, depth - 1, kNegativeInfinity, kPositiveInfinity, pov_side, local_stats);
    total_nodes += local_stats.nodes;
    total_prunes += local_stats.prunes;
    if (better_result(value, move, best_value, best_move)) {
      best_value = value;
      best_move = move;
    }
  }
#endif

  result.move = best_move;
  result.evaluation = best_value;
  result.stats.nodes += total_nodes;
  result.stats.prunes = total_prunes;
  result.threads_used = threads_used;
  return result;
}

int AlphaBetaEngine::Evaluate(const Board& board, int pov_side) const {
  const int p0_score = evaluate(board, static_cast<float>(pit_weight_));
  return pov_side == 0 ? p0_score : -p0_score;
}

int AlphaBetaEngine::Search(Board board, int depth, int alpha, int beta, int pov_side,
                            AlphaBetaStats& stats) const {
  ++stats.nodes;

  if (depth <= 0 || is_terminal(board)) {
    return Evaluate(board, pov_side);
  }

  const auto moves = legal_moves(board);
  if (moves.empty()) {
    return Evaluate(board, pov_side);
  }

  const bool maximizing = board.side_to_move == pov_side;
  int best = maximizing ? kNegativeInfinity : kPositiveInfinity;

  for (int move : moves) {
    bool extra_turn = false;
    Board child = apply_move(board, move, extra_turn);
    const int value = Search(child, depth - 1, alpha, beta, pov_side, stats);

    if (maximizing) {
      best = std::max(best, value);
      alpha = std::max(alpha, value);
    } else {
      best = std::min(best, value);
      beta = std::min(beta, value);
    }

    if (alpha >= beta) {
      ++stats.prunes;
      break;
    }
  }

  return best;
}
