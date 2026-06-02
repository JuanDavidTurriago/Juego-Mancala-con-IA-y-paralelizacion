#include "alphabeta.h"

#include <iostream>

namespace {

bool expect_true(bool condition, const char* message) {
  if (!condition) {
    std::cout << "FAIL: " << message << '\n';
    return false;
  }
  return true;
}

bool test_initial_position_has_legal_move() {
  Board board;
  AlphaBetaEngine engine;
  const auto result = engine.SearchBestMove(board, 4, 1);
  return expect_true(result.move >= 0 && result.move <= 5, "move must be a player 0 pit") &&
         expect_true(result.stats.nodes > 1, "search must visit nodes");
}

bool test_parallel_matches_sequential() {
  Board board;
  AlphaBetaEngine engine;
  const auto sequential = engine.SearchBestMove(board, 6, 1);
  const auto parallel = engine.SearchBestMove(board, 6, 4);
  return expect_true(parallel.move == sequential.move, "parallel move matches sequential") &&
         expect_true(parallel.evaluation == sequential.evaluation,
                     "parallel evaluation matches sequential") &&
         expect_true(parallel.threads_used >= 1, "threads_used is reported");
}

}  // namespace

int main() {
  int passed = 0;
  passed += test_initial_position_has_legal_move() ? 1 : 0;
  passed += test_parallel_matches_sequential() ? 1 : 0;
  std::cout << passed << "/2 tests passed\n";
  return passed == 2 ? 0 : 1;
}
