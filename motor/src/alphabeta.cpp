#include "alphabeta.h"

#include <algorithm> // std::max, std::min
#include <chrono>    // medicion de tiempo secuencial
#include <climits>   // INT_MIN, INT_MAX
#include <omp.h>
#include <vector>

namespace {

// Retorna true si candidate mejora a best para el jugador de la raiz.
bool better_root_value(int candidate_value, int candidate_move, int best_value, int best_move,
                       bool root_is_max) {
    if (best_move < 0) {
        return true;
    }
    if (candidate_value == best_value) {
        return candidate_move < best_move;
    }
    return root_is_max ? candidate_value > best_value : candidate_value < best_value;
}

}  // namespace

// Minimax puro sin poda; retorna el valor heuristico desde la perspectiva de P1.
int minimax(const Board& board, int depth, bool is_max) {
    if (depth == 0 || is_terminal(board)) {
        return evaluate(board);
    }

    std::vector<int> moves = legal_moves(board);
    if (moves.empty()) {
        return evaluate(board);
    }

    if (is_max) {
        int best = INT_MIN;
        for (int mv : moves) {
            bool extra = false;
            Board next = apply_move(board, mv, extra);
            bool next_is_max = extra ? is_max : !is_max;
            int val = minimax(next, depth - 1, next_is_max);
            best = std::max(best, val);
        }
        return best;
    }

    int best = INT_MAX;
    for (int mv : moves) {
        bool extra = false;
        Board next = apply_move(board, mv, extra);
        bool next_is_max = extra ? is_max : !is_max;
        int val = minimax(next, depth - 1, next_is_max);
        best = std::min(best, val);
    }
    return best;
}

// Retorna el hoyo optimo para el jugador activo usando Minimax puro.
int best_move_minimax(const Board& board, int depth) {
    if (depth == 0 || is_terminal(board)) {
        return -1;
    }

    const bool root_is_max = board.side_to_move == 0;
    int best_val = root_is_max ? INT_MIN : INT_MAX;
    int best_move = -1;

    for (int mv : legal_moves(board)) {
        bool extra = false;
        Board next = apply_move(board, mv, extra);
        bool next_is_max = extra ? root_is_max : !root_is_max;
        int val = minimax(next, depth - 1, next_is_max);
        if (better_root_value(val, mv, best_val, best_move, root_is_max)) {
            best_val = val;
            best_move = mv;
        }
    }

    return best_move;
}

// Alpha-Beta secuencial; retorna el valor heuristico y acumula nodos/cortes.
int alphabeta(const Board& board, int depth, int alpha, int beta,
              bool is_max, int& nodes, int& prunes) {
    nodes++;
    if (depth == 0 || is_terminal(board)) {
        return evaluate(board);
    }

    std::vector<int> moves = legal_moves(board);
    if (moves.empty()) {
        return evaluate(board);
    }

    if (is_max) {
        int best = INT_MIN;
        for (int mv : moves) {
            bool extra = false;
            Board next = apply_move(board, mv, extra);
            bool next_is_max = extra ? is_max : !is_max;
            int val = alphabeta(next, depth - 1, alpha, beta, next_is_max, nodes, prunes);
            best = std::max(best, val);
            alpha = std::max(alpha, best);
            if (alpha >= beta) {
                prunes++;
                break;
            }
        }
        return best;
    }

    int best = INT_MAX;
    for (int mv : moves) {
        bool extra = false;
        Board next = apply_move(board, mv, extra);
        bool next_is_max = extra ? is_max : !is_max;
        int val = alphabeta(next, depth - 1, alpha, beta, next_is_max, nodes, prunes);
        best = std::min(best, val);
        beta = std::min(beta, best);
        if (beta <= alpha) {
            prunes++;
            break;
        }
    }
    return best;
}

// Ejecuta Alpha-Beta desde la raiz, mide tiempo y retorna resultado completo.
ABResult best_move_alphabeta(const Board& board, int depth) {
    auto t0 = std::chrono::steady_clock::now();

    if (depth == 0 || is_terminal(board)) {
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return {-1, evaluate(board), 0, 0, ms};
    }

    int alpha = INT_MIN;
    int beta = INT_MAX;
    int total_nodes = 0;
    int total_prunes = 0;
    const bool root_is_max = board.side_to_move == 0;
    int best_val = root_is_max ? INT_MIN : INT_MAX;
    int best_move = -1;

    for (int mv : legal_moves(board)) {
        bool extra = false;
        Board next = apply_move(board, mv, extra);
        bool next_is_max = extra ? root_is_max : !root_is_max;
        int val = alphabeta(next, depth - 1, alpha, beta, next_is_max, total_nodes, total_prunes);

        if (better_root_value(val, mv, best_val, best_move, root_is_max)) {
            best_val = val;
            best_move = mv;
        }

        if (root_is_max) {
            alpha = std::max(alpha, val);
        } else {
            beta = std::min(beta, val);
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return {best_move, best_val, total_nodes, total_prunes, ms};
}

// Ejecuta Alpha-Beta con root parallelism; retorna mejor jugada y metricas agregadas.
ABParallelResult best_move_parallel(const Board& board, int depth, int nthreads) {
    const double t_start = omp_get_wtime();

    if (depth == 0 || is_terminal(board)) {
        const double t_end = omp_get_wtime();
        return {-1, evaluate(board), 0, 0, (t_end - t_start) * 1000.0, 1};
    }

    std::vector<int> moves = legal_moves(board);
    const int n = static_cast<int>(moves.size());
    if (n == 0) {
        const double t_end = omp_get_wtime();
        return {-1, evaluate(board), 0, 0, (t_end - t_start) * 1000.0, 1};
    }

    struct MoveResult {
        int move;
        int value;
        int nodes;
        int prunes;
    };

    std::vector<MoveResult> results(n);
    const bool root_is_max = board.side_to_move == 0;
    const int requested_threads = std::max(1, nthreads);
    int threads_used = 1;

#pragma omp parallel num_threads(requested_threads)
    {
#pragma omp single
        { threads_used = omp_get_num_threads(); }

#pragma omp for schedule(dynamic, 1)
        for (int i = 0; i < n; i++) {
            int local_nodes = 0;
            int local_prunes = 0;

            bool extra = false;
            Board next = apply_move(board, moves[i], extra);

            int local_alpha = INT_MIN;
            int local_beta = INT_MAX;
            bool child_is_max = extra ? root_is_max : !root_is_max;

            int val = alphabeta(next, depth - 1, local_alpha, local_beta,
                                child_is_max, local_nodes, local_prunes);

            results[i] = {moves[i], val, local_nodes, local_prunes};
        }
    }

    const double t_end = omp_get_wtime();

    int best_move_idx = -1;
    int best_value = root_is_max ? INT_MIN : INT_MAX;
    long long total_nodes = 0;
    long long total_prunes = 0;

    for (int i = 0; i < n; i++) {
        total_nodes += results[i].nodes;
        total_prunes += results[i].prunes;
        if (better_root_value(results[i].value, results[i].move, best_value, best_move_idx,
                              root_is_max)) {
            best_value = results[i].value;
            best_move_idx = results[i].move;
        }
    }

    return {
        best_move_idx,
        best_value,
        total_nodes,
        total_prunes,
        (t_end - t_start) * 1000.0,
        threads_used
    };
}
