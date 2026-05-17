#include "alphabeta.h"

#include <cassert>

int main() {
  Board b = Board::Initial();
  AlphaBetaEngine engine(0.5);
  auto res = engine.SearchBestMove(b, 1);
  assert(res.move >= 0 && res.move < Board::kPitsPerSide);
  return 0;
}

