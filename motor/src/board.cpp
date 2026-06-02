#include "board.h"

#include <cassert>

namespace {

bool is_player0_pit(int pit_index) {
  return pit_index >= 0 && pit_index <= 5;
}

bool is_player1_pit(int pit_index) {
  return pit_index >= 7 && pit_index <= 12;
}

bool is_terminal_position(const Board& board) {
  return side_pit_sum(board, 0) == 0 || side_pit_sum(board, 1) == 0;
}

void sweep_remaining_seeds(Board& board) {
  if (side_pit_sum(board, 0) == 0) {
    for (int pit = 7; pit <= 12; ++pit) {
      board.pits[kPlayer1Store] += board.pits[pit];
      board.pits[pit] = 0;
    }
  }

  if (side_pit_sum(board, 1) == 0) {
    for (int pit = 0; pit <= 5; ++pit) {
      board.pits[kPlayer0Store] += board.pits[pit];
      board.pits[pit] = 0;
    }
  }
}

Board swept_copy(Board board) {
  is_terminal(board);
  return board;
}

}  // namespace

Board::Board()
    : pits{4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0},
      side_to_move(0) {}

bool Board::operator==(const Board& other) const {
  return pits == other.pits && side_to_move == other.side_to_move;
}

bool is_valid_side(int side) {
  return side == 0 || side == 1;
}

bool is_player_pit(int side, int pit_index) {
  return (side == 0 && is_player0_pit(pit_index)) ||
         (side == 1 && is_player1_pit(pit_index));
}

int store_index(int side) {
  return side == 0 ? kPlayer0Store : kPlayer1Store;
}

int opponent_store_index(int side) {
  return side == 0 ? kPlayer1Store : kPlayer0Store;
}

int opposite_pit_index(int pit_index) {
  return 12 - pit_index;
}

int side_pit_sum(const Board& board, int side) {
  const int start = side == 0 ? 0 : 7;
  int sum = 0;
  for (int offset = 0; offset < 6; ++offset) {
    sum += board.pits[start + offset];
  }
  return sum;
}

int total_seed_count(const Board& board) {
  int total = 0;
  for (int seeds : board.pits) {
    total += seeds;
  }
  return total;
}

std::vector<int> legal_moves(const Board& board) {
  assert(is_valid_side(board.side_to_move));

  std::vector<int> moves;
  const int start = board.side_to_move == 0 ? 0 : 7;
  for (int offset = 0; offset < 6; ++offset) {
    const int pit_index = start + offset;
    if (board.pits[pit_index] > 0) {
      moves.push_back(pit_index);
    }
  }
  return moves;
}

Board apply_move(const Board& board, int pit_index, bool& extra_turn) {
  assert(is_valid_side(board.side_to_move));
  assert(is_player_pit(board.side_to_move, pit_index));
  assert(board.pits[pit_index] > 0);

#ifndef NDEBUG
  assert(total_seed_count(board) == kInitialSeedTotal);
#endif

  Board next = board;
  const int moving_side = board.side_to_move;
  int seeds = next.pits[pit_index];
  int current_index = pit_index;
  next.pits[pit_index] = 0;
  extra_turn = false;

  while (seeds > 0) {
    current_index = (current_index + 1) % kBoardSize;
    if (current_index == opponent_store_index(moving_side)) {
      continue;
    }
    ++next.pits[current_index];
    --seeds;
  }

  extra_turn = current_index == store_index(moving_side);

  const bool landed_in_empty_own_pit =
      !extra_turn && is_player_pit(moving_side, current_index) &&
      board.pits[current_index] == 0 && next.pits[current_index] == 1;
  if (landed_in_empty_own_pit) {
    const int opposite = opposite_pit_index(current_index);
    if (next.pits[opposite] > 0) {
      const int captured = next.pits[current_index] + next.pits[opposite];
      next.pits[current_index] = 0;
      next.pits[opposite] = 0;
      next.pits[store_index(moving_side)] += captured;
    }
  }

  if (!extra_turn) {
    next.side_to_move = 1 - moving_side;
  }

  is_terminal(next);

#ifndef NDEBUG
  assert(total_seed_count(next) == kInitialSeedTotal);
#endif

  return next;
}

bool is_terminal(const Board& board) {
  return is_terminal_position(board);
}

bool is_terminal(Board& board) {
  if (!is_terminal_position(board)) {
    return false;
  }
  sweep_remaining_seeds(board);
  return true;
}

int winner(const Board& board) {
  const Board final_board = swept_copy(board);
  if (final_board.pits[kPlayer0Store] > final_board.pits[kPlayer1Store]) {
    return 0;
  }
  if (final_board.pits[kPlayer1Store] > final_board.pits[kPlayer0Store]) {
    return 1;
  }
  return -1;
}

int evaluate(const Board& board, float pit_weight) {
  if (is_terminal(board)) {
    const int final_winner = winner(board);
    if (final_winner == 0) {
      return 1000000 + board.pits[kPlayer0Store] - board.pits[kPlayer1Store];
    }
    if (final_winner == 1) {
      return -1000000 + board.pits[kPlayer0Store] - board.pits[kPlayer1Store];
    }
    return 0;
  }

  const int store_diff = board.pits[kPlayer0Store] - board.pits[kPlayer1Store];
  const int pit_diff = side_pit_sum(board, 0) - side_pit_sum(board, 1);
  return static_cast<int>(store_diff + pit_weight * pit_diff);
}
