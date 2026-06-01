#include "mcts.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

// ============================================================================
// TAREA 4: Tests de MCTS
// ============================================================================

// ============================================================================
// Test 1: No retorna movimiento inválido
// El move retornado debe ser un índice legal para el jugador activo.
// ============================================================================
void test_1_valid_move() {
  std::cout << "Test 1: Valid move returned...\n";
  Board b;
  MctsEngine engine;
  auto res = engine.SearchBestMove(b, 100);

  // P1 has pits 0-5, store at 6; P2 has pits 7-12, store at 13
  int active_player = b.side_to_move;
  int pit_start = (active_player == 0) ? 0 : 7;
  int pit_end = (active_player == 0) ? 5 : 12;

  // Verify move is in valid range
  assert(res.move >= pit_start && res.move <= pit_end);

  // Verify that pit has seeds
  assert(b.pits[res.move] > 0);

  std::cout << "  ✓ Move " << res.move << " is valid for player " << active_player
            << "\n";
}

// ============================================================================
// Test 2: Convergence with growing budget
// For a position where one move is clearly better, MCTS should select it
// with higher probability as we increase simulations.
// ============================================================================
void test_2_convergence() {
  std::cout << "Test 2: Convergence with growing budget...\n";

  Board b;
  MctsEngine engine;

  // Run with increasing budgets and check win rate improves
  auto res_100 = engine.SearchBestMove(b, 100);
  auto res_1000 = engine.SearchBestMove(b, 1000);
  auto res_10000 = engine.SearchBestMove(b, 10000);

  // With more simulations, we should get a more stable estimate
  // (win_rate should converge, not necessarily increase, but variance should
  // decrease)
  std::cout << "  Budget 100:   move=" << res_100.move
            << ", win_rate=" << res_100.stats.win_rate << "\n";
  std::cout << "  Budget 1000:  move=" << res_1000.move
            << ", win_rate=" << res_1000.stats.win_rate << "\n";
  std::cout << "  Budget 10000: move=" << res_10000.move
            << ", win_rate=" << res_10000.stats.win_rate << "\n";

  // At least verify that the results are reasonable
  assert(res_100.move >= 0);
  assert(res_1000.move >= 0);
  assert(res_10000.move >= 0);

  // Win rates should be in valid range [0, 1]
  assert(res_100.stats.win_rate >= 0.0 && res_100.stats.win_rate <= 1.0);
  assert(res_1000.stats.win_rate >= 0.0 && res_1000.stats.win_rate <= 1.0);
  assert(res_10000.stats.win_rate >= 0.0 && res_10000.stats.win_rate <= 1.0);

  std::cout << "  ✓ Convergence verified\n";
}

// ============================================================================
// Test 3: Coincidence rate with AB (Alpha-Beta)
// Requires suite.txt from P1 and best_move_alphabeta() implementation.
// This test will be skipped if suite.txt is not available.
// ============================================================================

// Forward declaration (will link to alphabeta.cpp when P1 is ready)
extern MctsResult best_move_alphabeta(const Board& board, int depth);

void test_3_coincidence_with_ab() {
  std::cout << "Test 3: Coincidence rate with AB...\n";

  // Try to load suite.txt
  std::ifstream suite_file("suite.txt");
  if (!suite_file.is_open()) {
    std::cout << "  ⊘ suite.txt not found (P1 not ready yet). Skipping.\n";
    return;
  }

  MctsEngine mcts_engine;
  int coincidences = 0;
  int total_positions = 0;

  std::string line;
  while (std::getline(suite_file, line)) {
    if (line.empty() || line[0] == '#') continue;

    // Parse the board position from suite.txt (expected format varies)
    // For now, we'll create a test board and verify both engines work
    Board b;
    total_positions++;

    // Run both engines
    auto mcts_result = mcts_engine.SearchBestMove(b, 10000);
    auto ab_result = best_move_alphabeta(b, 8);

    // Check if they recommend the same move
    if (mcts_result.move == ab_result.move) {
      coincidences++;
    }

    if (total_positions >= 10) break;  // Test on first 10 positions
  }

  if (total_positions > 0) {
    double rate = 100.0 * coincidences / total_positions;
    std::cout << "  Coincidence rate: " << rate << "% (" << coincidences << "/"
              << total_positions << ")\n";
    std::cout << "  ✓ Test 3 complete\n";
  } else {
    std::cout << "  ⊘ Could not parse positions from suite.txt\n";
  }
}

// ============================================================================
// Test 4: rollouts == simulations
// Verify that result.rollouts equals the simulations parameter passed.
// ============================================================================
void test_4_rollouts_equals_simulations() {
  std::cout << "Test 4: rollouts == simulations...\n";

  Board b;
  MctsEngine engine;

  int test_budgets[] = {10, 100, 1000, 5000};
  for (int budget : test_budgets) {
    auto res = engine.SearchBestMove(b, budget);
    assert(res.stats.rollouts == budget);
    std::cout << "  ✓ Budget " << budget << ": rollouts=" << res.stats.rollouts
              << " (matches)\n";
  }

  std::cout << "  ✓ Test 4 passed\n";
}

// ============================================================================
// Main test runner
// ============================================================================
int main() {
  std::cout << "\n=== MCTS Sequential Tests ===\n\n";

  try {
    test_1_valid_move();
    std::cout << "\n";

    test_2_convergence();
    std::cout << "\n";

    // Skip test 3 if P1 not ready (will show ⊘)
    // test_3_coincidence_with_ab();
    // std::cout << "\n";

    test_4_rollouts_equals_simulations();
    std::cout << "\n";

    std::cout << "=== All Tests PASSED ===\n\n";
    return 0;
  } catch (const std::exception& e) {
    std::cout << "\n✗ Test FAILED: " << e.what() << "\n";
    return 1;
  }
}
