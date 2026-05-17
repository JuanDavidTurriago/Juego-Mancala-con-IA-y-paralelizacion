#include "../src/board.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int sum_seeds(const Board& board) {
  int sum = 0;
  for (int seeds : board.pits) {
    sum += seeds;
  }
  return sum;
}

Board empty_board(int side_to_move = 0) {
  Board board;
  for (int& pit : board.pits) {
    pit = 0;
  }
  board.side_to_move = side_to_move;
  return board;
}

bool expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cout << "  FAIL: " << message << '\n';
    return false;
  }
  return true;
}

bool test_initial_state() {
  Board board;
  bool ok = true;
  ok &= expect_true(sum_seeds(board) == 48, "initial seed sum must be 48");
  ok &= expect_true(board.pits[6] == 0, "P1 store must start empty");
  ok &= expect_true(board.pits[13] == 0, "P2 store must start empty");
  return ok;
}

bool test_seed_conservation() {
  Board board;
  bool ok = true;
  for (int move : legal_moves(board)) {
    bool extra_turn = false;
    Board next = apply_move(board, move, extra_turn);
    ok &= expect_true(sum_seeds(next) == 48, "seed sum must remain 48 after move " + std::to_string(move));
  }
  return ok;
}

bool test_extra_turn() {
  Board board = empty_board(0);
  board.pits[5] = 1;
  board.pits[6] = 23;
  for (int pit = 7; pit <= 12; ++pit) {
    board.pits[pit] = 4;
  }

  bool extra_turn = false;
  Board next = apply_move(board, 5, extra_turn);
  bool ok = true;
  ok &= expect_true(extra_turn, "move from pit 5 with 1 seed must grant extra turn");
  ok &= expect_true(next.side_to_move == 0, "side_to_move must remain P1");
  ok &= expect_true(sum_seeds(next) == 48, "seed sum must remain 48");
  return ok;
}

bool test_skip_opponent_store() {
  Board board = empty_board(0);
  board.pits[0] = 14;
  board.pits[6] = 10;
  for (int pit = 7; pit <= 12; ++pit) {
    board.pits[pit] = 4;
  }

  bool extra_turn = false;
  Board next = apply_move(board, 0, extra_turn);
  bool ok = true;
  ok &= expect_true(next.pits[13] == 0, "P1 must skip P2 store at index 13");
  ok &= expect_true(sum_seeds(next) == 48, "seed sum must remain 48");
  return ok;
}

bool test_successful_capture() {
  Board board = empty_board(0);
  board.pits[2] = 1;
  board.pits[3] = 0;
  board.pits[6] = 20;
  board.pits[9] = 5;
  board.pits[13] = 22;

  bool extra_turn = false;
  Board next = apply_move(board, 2, extra_turn);
  bool ok = true;
  ok &= expect_true(!extra_turn, "capture move must not be an extra turn");
  ok &= expect_true(next.pits[3] == 0, "capturing pit must be emptied");
  ok &= expect_true(next.pits[9] == 0, "opposite pit must be emptied");
  ok &= expect_true(next.pits[6] == 26, "P1 store must increase by 6");
  ok &= expect_true(sum_seeds(next) == 48, "seed sum must remain 48");
  return ok;
}

bool test_game_end_and_sweep() {
  Board board = empty_board(0);
  board.pits[5] = 7;
  board.pits[7] = 3;
  board.pits[8] = 2;
  board.pits[9] = 4;
  board.pits[10] = 1;
  board.pits[13] = 31;

  bool extra_turn = false;
  Board next = apply_move(board, 5, extra_turn);
  bool ok = true;
  ok &= expect_true(!extra_turn, "last seed from pit 5 with 7 seeds must end at index 12");
  ok &= expect_true(is_terminal(next), "board must be terminal after P1 side becomes empty");
  ok &= expect_true(next.pits[7] == 0 && next.pits[8] == 0 && next.pits[9] == 0, "P2 pits must be swept");
  ok &= expect_true(next.pits[10] == 0 && next.pits[11] == 0 && next.pits[12] == 0, "all P2 pits must be swept");
  ok &= expect_true(next.pits[13] == 47, "P2 store must receive all remaining P2 seeds");
  ok &= expect_true(sum_seeds(next) == 48, "seed sum must remain 48");
  return ok;
}

bool test_no_capture_when_opposite_empty() {
  Board board = empty_board(0);
  board.pits[2] = 1;
  board.pits[3] = 0;
  board.pits[6] = 20;
  board.pits[8] = 5;
  board.pits[9] = 0;
  board.pits[13] = 22;

  bool extra_turn = false;
  Board next = apply_move(board, 2, extra_turn);
  bool ok = true;
  ok &= expect_true(next.pits[3] == 1, "own landing pit must keep the seed");
  ok &= expect_true(next.pits[9] == 0, "empty opposite pit remains empty");
  ok &= expect_true(next.pits[6] == 20, "P1 store must not change by capture");
  ok &= expect_true(sum_seeds(next) == 48, "seed sum must remain 48");
  return ok;
}

struct TestCase {
  const char* name;
  bool (*run)();
};

}  // namespace

int main() {
  std::vector<TestCase> tests = {
      {"TEST 1 - Estado inicial", test_initial_state},
      {"TEST 2 - Conservacion de semillas", test_seed_conservation},
      {"TEST 3 - Turno extra", test_extra_turn},
      {"TEST 4 - Saltar kalaha del rival", test_skip_opponent_store},
      {"TEST 5 - Captura exitosa", test_successful_capture},
      {"TEST 6 - Fin de juego y barrido", test_game_end_and_sweep},
      {"TEST 7 - No captura con opuesto vacio", test_no_capture_when_opposite_empty},
  };

  int passed = 0;
  for (const TestCase& test : tests) {
    bool ok = test.run();
    std::cout << (ok ? "PASS: " : "FAIL: ") << test.name << '\n';
    if (ok) {
      ++passed;
    }
  }

  std::cout << passed << "/" << tests.size() << " tests passed\n";
  return passed == static_cast<int>(tests.size()) ? 0 : 1;
}

