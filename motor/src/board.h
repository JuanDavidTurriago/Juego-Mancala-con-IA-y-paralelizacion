#pragma once

#include <vector>

struct Board {
  int pits[14];
  int side_to_move;  // 0 = P1, 1 = P2

  // Creates the initial Kalah(6,4) position with 4 seeds in each pit and empty stores.
  Board();

  // Prints the board in a compact console format for debugging.
  void display() const;

  // Returns true when both boards have the same pits and active player.
  bool operator==(const Board& other) const;
};

// Returns legal pit indices for the active player; only pits with seeds are included.
std::vector<int> legal_moves(const Board& board);

// Applies a legal pit move to a copy of board and returns the new state.
// Sets extra_turn to true when the last seed lands in the moving player's store.
Board apply_move(const Board& board, int pit_index, bool& extra_turn);

// Returns true when all pits on either side are empty.
bool is_terminal(const Board& board);

// Mutable overload used by game progression; applies the terminal sweep in-place.
bool is_terminal(Board& board);

// Returns 0 if P1 wins, 1 if P2 wins, and -1 for a draw.
int winner(const Board& board);

// Evaluates the board from P1 perspective; positive favors P1, negative favors P2.
int evaluate(const Board& board, float alpha = 0.5f);
