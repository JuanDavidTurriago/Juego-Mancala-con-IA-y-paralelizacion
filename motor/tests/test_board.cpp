#include "board.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

Board empty_board(int side_to_move = 0) {
  Board board;
  board.pits.fill(0);
  board.side_to_move = side_to_move;
  return board;
}

bool expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cout << "FAIL: " << message << '\n';
    return false;
  }
  return true;
}

bool test_initial_state() {
  Board board;
  bool ok = true;
  ok &= expect_true(total_seed_count(board) == 48, "initial seed sum must be 48");
  ok &= expect_true(board.pits[6] == 0, "player 0 store must start empty");
  ok &= expect_true(board.pits[13] == 0, "player 1 store must start empty");
  for (int pit = 0; pit <= 5; ++pit) {
    ok &= expect_true(board.pits[pit] == 4, "player 0 pits start with 4 seeds");
  }
  for (int pit = 7; pit <= 12; ++pit) {
    ok &= expect_true(board.pits[pit] == 4, "player 1 pits start with 4 seeds");
  }
  return ok;
}

bool test_seed_conservation() {
  Board board;
  bool ok = true;
  for (int move : legal_moves(board)) {
    bool extra_turn = false;
    Board next = apply_move(board, move, extra_turn);
    ok &= expect_true(total_seed_count(next) == 48, "seed sum remains 48");
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
  ok &= expect_true(extra_turn, "last seed in own store grants extra turn");
  ok &= expect_true(next.side_to_move == 0, "side remains player 0");
  ok &= expect_true(total_seed_count(next) == 48, "seed sum remains 48");
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
  ok &= expect_true(next.pits[13] == 0, "player 0 skips player 1 store");
  ok &= expect_true(total_seed_count(next) == 48, "seed sum remains 48");
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
  ok &= expect_true(!extra_turn, "capture is not an extra turn");
  ok &= expect_true(next.pits[3] == 0, "landing pit is emptied after capture");
  ok &= expect_true(next.pits[9] == 0, "opposite pit is emptied after capture");
  ok &= expect_true(next.pits[6] == 26, "captured seeds go to own store");
  ok &= expect_true(total_seed_count(next) == 48, "seed sum remains 48");
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
  ok &= expect_true(is_terminal(next), "board is terminal after one side is empty");
  for (int pit = 7; pit <= 12; ++pit) {
    ok &= expect_true(next.pits[pit] == 0, "remaining player 1 pits are swept");
  }
  ok &= expect_true(next.pits[13] == 47, "remaining seeds go to player 1 store");
  ok &= expect_true(total_seed_count(next) == 48, "seed sum remains 48");
  return ok;
}

}  // namespace

int main() {
  const std::vector<std::pair<std::string, bool (*)()>> tests = {
      {"initial_state", test_initial_state},
      {"seed_conservation", test_seed_conservation},
      {"extra_turn", test_extra_turn},
      {"skip_opponent_store", test_skip_opponent_store},
      {"successful_capture", test_successful_capture},
      {"game_end_and_sweep", test_game_end_and_sweep},
  };

  int passed = 0;
  for (const auto& test : tests) {
    const bool ok = test.second();
    std::cout << (ok ? "PASS " : "FAIL ") << test.first << '\n';
    if (ok) {
      ++passed;
    }
  }

  std::cout << passed << "/" << tests.size() << " tests passed\n";
  return passed == static_cast<int>(tests.size()) ? 0 : 1;
}
