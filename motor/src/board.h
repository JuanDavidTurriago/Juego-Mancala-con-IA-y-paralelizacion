#pragma once

#include <vector>

struct Board {
  int pits[14];
  int side_to_move = 0;

  // Inicializa el estado inicial de Kalah(6,4).
  Board();

  // Imprime una vista simple del tablero para depuracion en consola.
  void display() const;

  // Compara pits y jugador activo; util para pruebas unitarias.
  bool operator==(const Board& other) const;
};

constexpr int kPlayer0Store = 6;
constexpr int kPlayer1Store = 13;
constexpr int kBoardSize = 14;
constexpr int kInitialSeedTotal = 48;

// Retorna true si side representa un jugador valido.
bool is_valid_side(int side);

// Retorna true si pit_index pertenece al jugador indicado.
bool is_player_pit(int side, int pit_index);

// Retorna el indice del kalaha del jugador indicado.
int store_index(int side);

// Retorna el indice del kalaha del rival del jugador indicado.
int opponent_store_index(int side);

// Retorna el hoyo opuesto usado por la regla de captura.
int opposite_pit_index(int pit_index);

// Suma las semillas en los seis hoyos regulares de un jugador.
int side_pit_sum(const Board& board, int side);

// Suma todas las semillas del tablero, incluidos los dos kalahas.
int total_seed_count(const Board& board);

// Retorna los indices de hoyos legales para el jugador activo.
std::vector<int> legal_moves(const Board& board);

// Aplica un movimiento legal sobre una copia y retorna el nuevo estado.
Board apply_move(const Board& board, int pit_index, bool& extra_turn);

// Retorna true si alguna fila de hoyos regulares esta vacia.
bool is_terminal(const Board& board);

// Retorna true si el juego termino y normaliza el tablero con el barrido final.
bool is_terminal(Board& board);

// Retorna 0 si gana P1, 1 si gana P2 o -1 si hay empate.
int winner(const Board& board);

// Evalua el tablero: positivo favorece P1, negativo favorece P2.
int evaluate(const Board& board, float pit_weight = 0.5f);
