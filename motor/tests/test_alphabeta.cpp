#include "alphabeta.h"

#include <cassert>

int main() {
  Board b;
  AlphaBetaEngine engine(0.5);
  auto res = engine.SearchBestMove(b, 1);
  assert(res.move >= 0 && res.move <= 5);
  return 0;
}
