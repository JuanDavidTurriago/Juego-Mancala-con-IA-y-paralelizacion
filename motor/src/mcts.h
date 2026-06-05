#pragma once

#include "board.h"

// ─────────────────────────────────────────────────────────────────────────────
// Resultado que devuelve best_move_mcts().
// ─────────────────────────────────────────────────────────────────────────────
struct MCTSResult {
  int    move           = -1;   ///< Índice del hoyo elegido (0-5 P0, 7-12 P1)
  double win_rate       = 0.0;  ///< w / n del hijo raíz elegido
  int    rollouts       = 0;    ///< Número de simulaciones completadas (== simulations)
  double tree_depth_avg = 0.0;  ///< Profundidad promedio de los rollouts
};

// ─────────────────────────────────────────────────────────────────────────────
// Función principal de MCTS con UCT.
//   board       – posición de partida (const-ref, sin copias adicionales)
//   simulations – número de iteraciones del ciclo selección→rollout→back-prop
//   c           – constante de exploración UCT (recomendado √2 ≈ 1.41421f)
// ─────────────────────────────────────────────────────────────────────────────
MCTSResult best_move_mcts(const Board& board, int simulations, float c);

// ─────────────────────────────────────────────────────────────────────────────
// Resultado que devuelve best_move_mcts_parallel().
// ─────────────────────────────────────────────────────────────────────────────
struct MCTSParallelResult {
  int    move           = -1;
  float  win_rate       = 0.0f;
  int    rollouts       = 0;
  float  tree_depth_avg = 0.0f;
  double elapsed_ms     = 0.0;
  int    threads_used   = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Función MCTS paralela (Root Parallelization)
//   seed_base   – semilla base (opcional, útil para determinismo en tests)
// ─────────────────────────────────────────────────────────────────────────────
MCTSParallelResult best_move_mcts_parallel(const Board& board, int simulations, int nthreads, float c = 1.41421f, int seed_base = -1);
