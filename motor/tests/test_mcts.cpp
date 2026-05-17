#include "mcts.h"

#include <cassert>

int main() {
  Board b = Board::Initial();
  MctsEngine engine;
  auto res = engine.SearchBestMove(b, 10);
  assert(res.move >= 0 && res.move < Board::kPitsPerSide);
  return 0;
}

