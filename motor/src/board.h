#pragma once

#include <array>
#include <vector>

struct Board {
  std::array<int, 14> pits{};
  int side_to_move = 0;

  Board();

  bool operator==(const Board& other) const;
};

constexpr int kPlayer0Store = 6;
constexpr int kPlayer1Store = 13;
constexpr int kBoardSize = 14;
constexpr int kInitialSeedTotal = 48;

bool is_valid_side(int side);
bool is_player_pit(int side, int pit_index);
int store_index(int side);
int opponent_store_index(int side);
int opposite_pit_index(int pit_index);
int side_pit_sum(const Board& board, int side);
int total_seed_count(const Board& board);

std::vector<int> legal_moves(const Board& board);
Board apply_move(const Board& board, int pit_index, bool& extra_turn);
bool is_terminal(const Board& board);
bool is_terminal(Board& board);
int winner(const Board& board);
int evaluate(const Board& board, float pit_weight = 0.5f);
