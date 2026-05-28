#include "mcts.h"

#include <cassert>

int main() {
  Board b;
  MctsEngine engine;
  auto res = engine.SearchBestMove(b, 10);
  assert(res.move >= 0 && res.move <= 5);
  return 0;
}
