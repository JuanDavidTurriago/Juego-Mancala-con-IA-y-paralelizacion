#include "board.h"

#include <cassert>
#include <climits>
#include <iostream>

namespace {

constexpr int kTotalSeeds = 48;
constexpr int kP1Store = 6;
constexpr int kP2Store = 13;

// Returns true when side is a valid player id.
bool is_valid_side(int side) {
  return side == 0 || side == 1;
}

// Returns true when pit_index belongs to P1's regular pits.
bool is_p1_pit(int pit_index) {
  return pit_index >= 0 && pit_index <= 5;
}

// Returns true when pit_index belongs to P2's regular pits.
bool is_p2_pit(int pit_index) {
  return pit_index >= 7 && pit_index <= 12;
}

// Returns true when pit_index is a regular pit owned by side.
bool owns_pit(int side, int pit_index) {
  return (side == 0 && is_p1_pit(pit_index)) || (side == 1 && is_p2_pit(pit_index));
}

// Returns true when pit_index is a playable pit for side.
bool is_legal_pit_for_side(int side, int pit_index) {
  return owns_pit(side, pit_index);
}

// Returns the store index for side.
int store_index(int side) {
  return side == 0 ? kP1Store : kP2Store;
}

// Returns the opponent store index for side.
int opponent_store_index(int side) {
  return side == 0 ? kP2Store : kP1Store;
}

// Returns the opposite pit index used by the capture rule.
int opposite_pit_index(int pit_index) {
  return 12 - pit_index;
}

// Returns the total number of seeds currently stored in all 14 positions.
int total_seed_count(const Board& board) {
  int total = 0;
  for (int seeds : board.pits) {
    total += seeds;
  }
  return total;
}

// Returns the number of seeds in the six regular pits for side.
int side_pit_sum(const Board& board, int side) {
  int start = side == 0 ? 0 : 7;
  int sum = 0;
  for (int offset = 0; offset < 6; ++offset) {
    sum += board.pits[start + offset];
  }
  return sum;
}

// Moves all remaining seeds to stores when one side has no regular pit seeds.
void sweep_remaining_seeds(Board& board) {
  if (side_pit_sum(board, 0) == 0) {
    for (int pit = 7; pit <= 12; ++pit) {
      board.pits[kP2Store] += board.pits[pit];
      board.pits[pit] = 0;
    }
  }

  if (side_pit_sum(board, 1) == 0) {
    for (int pit = 0; pit <= 5; ++pit) {
      board.pits[kP1Store] += board.pits[pit];
      board.pits[pit] = 0;
    }
  }
}

// Returns a copied board with terminal sweep applied when needed.
Board swept_copy(Board board) {
  if (is_terminal(board)) {
    sweep_remaining_seeds(board);
  }
  return board;
}

}  // namespace

Board::Board() : pits{4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0}, side_to_move(0) {}

void Board::display() const {
  std::cout << "P2: ";
  for (int pit = 12; pit >= 7; --pit) {
    std::cout << pits[pit] << ' ';
  }
  std::cout << "\nStores P2=" << pits[kP2Store] << " P1=" << pits[kP1Store] << '\n';
  std::cout << "P1: ";
  for (int pit = 0; pit <= 5; ++pit) {
    std::cout << pits[pit] << ' ';
  }
  std::cout << "\nSide to move: P" << (side_to_move + 1) << '\n';
}

bool Board::operator==(const Board& other) const {
  if (side_to_move != other.side_to_move) {
    return false;
  }

  for (int i = 0; i < 14; ++i) {
    if (pits[i] != other.pits[i]) {
      return false;
    }
  }

  return true;
}

std::vector<int> legal_moves(const Board& board) {
  assert(is_valid_side(board.side_to_move));

  std::vector<int> moves;
  int start = board.side_to_move == 0 ? 0 : 7;
  for (int offset = 0; offset < 6; ++offset) {
    int pit_index = start + offset;
    if (board.pits[pit_index] > 0) {
      moves.push_back(pit_index);
    }
  }
  return moves;
}

Board apply_move(const Board& board, int pit_index, bool& extra_turn) {
  assert(is_valid_side(board.side_to_move));
  assert(is_legal_pit_for_side(board.side_to_move, pit_index));
  assert(board.pits[pit_index] > 0);

#ifndef NDEBUG
  int initial_total_seeds = total_seed_count(board);
  assert(initial_total_seeds == kTotalSeeds && "Invariante rota: semillas perdidas o creadas");
#endif

  Board next = board;
  int moving_side = board.side_to_move;
  int seeds = next.pits[pit_index];
  int current_index = pit_index;

  next.pits[pit_index] = 0;
  extra_turn = false;

  while (seeds > 0) {
    current_index = (current_index + 1) % 14;
    if (current_index == opponent_store_index(moving_side)) {
      continue;
    }

    next.pits[current_index] += 1;
    --seeds;
  }

  extra_turn = current_index == store_index(moving_side);

  if (!extra_turn && owns_pit(moving_side, current_index) && board.pits[current_index] == 0 &&
      next.pits[current_index] == 1) {
    int opposite_index = opposite_pit_index(current_index);
    if (next.pits[opposite_index] > 0) {
      int captured = next.pits[current_index] + next.pits[opposite_index];
      next.pits[current_index] = 0;
      next.pits[opposite_index] = 0;
      next.pits[store_index(moving_side)] += captured;
    }
  }

  if (!extra_turn) {
    next.side_to_move = 1 - moving_side;
  }

  if (is_terminal(next)) {
    sweep_remaining_seeds(next);
  }

#ifndef NDEBUG
  int total_seeds = total_seed_count(next);
  assert(total_seeds == 48 && "Invariante rota: semillas perdidas o creadas");
#endif

  return next;
}

bool is_terminal(const Board& board) {
  return side_pit_sum(board, 0) == 0 || side_pit_sum(board, 1) == 0;
}

int winner(const Board& board) {
  Board final_board = swept_copy(board);
  if (final_board.pits[kP1Store] > final_board.pits[kP2Store]) {
    return 0;
  }
  if (final_board.pits[kP2Store] > final_board.pits[kP1Store]) {
    return 1;
  }
  return -1;
}

int evaluate(const Board& board, float alpha) {
  if (is_terminal(board)) {
    int final_winner = winner(board);
    if (final_winner == 0) {
      return INT_MAX;
    }
    if (final_winner == 1) {
      return INT_MIN;
    }
    return 0;
  }

  int store_diff = board.pits[kP1Store] - board.pits[kP2Store];
  int pit_diff = side_pit_sum(board, 0) - side_pit_sum(board, 1);
  return static_cast<int>(static_cast<float>(store_diff) + alpha * pit_diff);
}
