#include "alphabeta.h"
#include "mcts.h"

#include <iostream>

int main(int argc, char** argv) {
  // Placeholder benchmark binary; flags and instrumentation are added in later phases.
  (void)argc;
  (void)argv;
  Board b;
  AlphaBetaEngine ab(0.5);
  auto r1 = ab.SearchBestMove(b, 1);
  std::cout << "alphabeta move=" << r1.move << " eval=" << r1.evaluation << "\n";
  MctsEngine mcts;
  auto r2 = mcts.SearchBestMove(b, 100);
  std::cout << "mcts move=" << r2.move << " winrate=" << r2.evaluation << "\n";
  return 0;
}
