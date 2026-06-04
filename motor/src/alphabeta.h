#pragma once
#include "board.h"

struct ABResult {
    int move;          // indice del hoyo elegido (0-5 P1, 7-12 P2)
    int value;         // valor heuristico del nodo raiz
    int nodes;         // total de nodos explorados en la busqueda
    int prunes;        // total de cortes realizados
    double elapsed_ms; // tiempo de pared en milisegundos
};

// Minimax puro sin poda; retorna el valor heuristico del nodo.
int minimax(const Board& board, int depth, bool is_max);

// Retorna el indice del hoyo optimo segun Minimax puro a la profundidad dada.
int best_move_minimax(const Board& board, int depth);

// Alpha-Beta secuencial; retorna el valor heuristico y acumula nodos/cortes.
int alphabeta(const Board& board, int depth, int alpha, int beta,
              bool is_max, int& nodes, int& prunes);

// Ejecuta Alpha-Beta desde la raiz, mide tiempo y retorna la mejor jugada.
ABResult best_move_alphabeta(const Board& board, int depth);

// -- Alpha-Beta con Root Parallelism (OpenMP) -------------------------------

struct ABParallelResult {
    int move;           // indice del hoyo elegido
    int value;          // valor heuristico del nodo raiz
    long long nodes;    // total de nodos explorados (suma de todos los hilos)
    long long prunes;   // total de podas (suma de todos los hilos)
    double elapsed_ms;  // tiempo de pared medido con omp_get_wtime()
    int threads_used;   // hilos realmente utilizados
};

// Root parallelism: reparte los movimientos de la raiz entre hilos.
// Cada hilo ejecuta alphabeta() secuencial sobre su subarbol asignado.
ABParallelResult best_move_parallel(const Board& board, int depth, int nthreads);
