#include "mcts.h"

MctsEngine::MctsEngine(double exploration_c) : exploration_c_(exploration_c) {
  (void)exploration_c_;
}

MctsResult MctsEngine::SearchBestMove(const Board& board, int simulations) {
  // Placeholder skeleton: real MCTS will be implemented in fase correspondiente.
  // For now, return the first legal move (if any) so the project compiles.
  MctsResult result;
  result.stats.rollouts = simulations;

  auto moves = legal_moves(board);
  if (!moves.empty()) result.move = moves.front();
  result.evaluation = 0.0;
  result.stats.win_rate = 0.0;
  result.stats.tree_depth_avg = 0.0;
  return result;
}
