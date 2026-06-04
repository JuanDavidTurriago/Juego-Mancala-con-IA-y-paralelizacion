#include "alphabeta.h"

#include <algorithm>
#include <climits>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool expect_true(bool condition, const std::string& message) {
    if (!condition) {
        std::cout << "  FAIL: " << message << '\n';
        return false;
    }
    return true;
}

Board empty_board(int side_to_move = 0) {
    Board board;
    board.pits.fill(0);
    board.side_to_move = side_to_move;
    return board;
}

std::vector<int> parse_position_values(std::string line) {
    std::replace(line.begin(), line.end(), ',', ' ');
    std::stringstream ss(line);
    std::vector<int> values;
    int value = 0;
    while (ss >> value) {
        values.push_back(value);
    }
    return values;
}

std::vector<Board> load_suite_positions(const std::string& path) {
    std::ifstream in(path);
    std::vector<Board> positions;
    std::string line;
    while (std::getline(in, line)) {
        const auto first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || line[first] == '#') {
            continue;
        }

        const std::vector<int> values = parse_position_values(line);
        if (values.size() != 14 && values.size() != 15) {
            continue;
        }

        Board board;
        for (int i = 0; i < kBoardSize; ++i) {
            board.pits[i] = values[i];
        }
        board.side_to_move = values.size() == 15 ? values[14] : 0;
        if (total_seed_count(board) == kInitialSeedTotal && is_valid_side(board.side_to_move)) {
            positions.push_back(board);
        }
    }

    if (positions.empty()) {
        positions.push_back(Board{});
    }
    return positions;
}

std::vector<Board> sample_positions() {
    std::vector<Board> positions;
    Board current;
    positions.push_back(current);

    for (int step = 0; step < 9; ++step) {
        std::vector<int> moves = legal_moves(current);
        bool extra = false;
        int move = moves[step % static_cast<int>(moves.size())];
        current = apply_move(current, move, extra);
        positions.push_back(current);
    }

    return positions;
}

int counted_minimax(const Board& board, int depth, bool is_max, int& nodes) {
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
            int val = counted_minimax(next, depth - 1, next_is_max, nodes);
            best = std::max(best, val);
        }
        return best;
    }

    int best = INT_MAX;
    for (int mv : moves) {
        bool extra = false;
        Board next = apply_move(board, mv, extra);
        bool next_is_max = extra ? is_max : !is_max;
        int val = counted_minimax(next, depth - 1, next_is_max, nodes);
        best = std::min(best, val);
    }
    return best;
}

int counted_best_move_minimax(const Board& board, int depth, int& nodes) {
    if (depth == 0 || is_terminal(board)) {
        nodes++;
        return -1;
    }

    const bool root_is_max = board.side_to_move == 0;
    int best_val = root_is_max ? INT_MIN : INT_MAX;
    int best_move = -1;

    nodes++;
    for (int mv : legal_moves(board)) {
        bool extra = false;
        Board next = apply_move(board, mv, extra);
        bool next_is_max = extra ? root_is_max : !root_is_max;
        int val = counted_minimax(next, depth - 1, next_is_max, nodes);
        const bool better = best_move < 0 || (root_is_max && val > best_val) ||
                            (!root_is_max && val < best_val);
        if (better) {
            best_val = val;
            best_move = mv;
        }
    }

    return best_move;
}

bool test_minimax_equals_alphabeta() {
    std::vector<Board> positions = sample_positions();
    int depths[] = {4, 6};
    bool ok = true;

    for (int i = 0; i < static_cast<int>(positions.size()); ++i) {
        for (int depth : depths) {
            int mm_move = best_move_minimax(positions[i], depth);
            ABResult ab = best_move_alphabeta(positions[i], depth);
            ok &= expect_true(
                mm_move == ab.move,
                "position " + std::to_string(i + 1) + ", depth " + std::to_string(depth) +
                    ": minimax=" + std::to_string(mm_move) + ", alphabeta=" + std::to_string(ab.move));
        }
    }

    return ok;
}

bool test_extra_turn_respected() {
    Board board = empty_board(0);
    board.pits[5] = 1;
    board.pits[kPlayer0Store] = 24;
    board.pits[7] = 4;
    board.pits[8] = 4;
    board.pits[9] = 4;
    board.pits[10] = 4;
    board.pits[11] = 4;
    board.pits[12] = 3;

    ABResult result = best_move_alphabeta(board, 4);
    bool ok = true;
    ok &= expect_true(result.move == 5, "best move must be pit 5 because it grants extra turn");
    ok &= expect_true(result.value > 0, "extra-turn continuation must favor P1");
    return ok;
}

bool test_terminal_returns_extreme_value() {
    Board board = empty_board(0);
    board.pits[kPlayer0Store] = 30;
    for (int pit = 7; pit <= 12; ++pit) {
        board.pits[pit] = 3;
    }

    ABResult result = best_move_alphabeta(board, 5);
    bool ok = true;
    ok &= expect_true(is_terminal(board), "manual board must be terminal");
    ok &= expect_true(result.value > 0, "P1 terminal win must be positive");
    return ok;
}

bool test_prunes_positive() {
    Board board;
    ABResult result = best_move_alphabeta(board, 8);
    return expect_true(result.prunes > 0, "initial position at depth 8 must produce at least one prune");
}

bool test_alphabeta_explores_fewer_nodes() {
    Board board;
    int minimax_nodes = 0;
    int mm_move = counted_best_move_minimax(board, 6, minimax_nodes);
    ABResult ab = best_move_alphabeta(board, 6);

    bool ok = true;
    ok &= expect_true(mm_move == ab.move, "counted minimax and alphabeta must choose the same move");
    ok &= expect_true(ab.nodes < minimax_nodes,
                      "alphabeta nodes (" + std::to_string(ab.nodes) +
                          ") must be less than minimax nodes (" + std::to_string(minimax_nodes) + ")");
    return ok;
}

bool test_parallel_matches_sequential() {
    std::vector<Board> positions = load_suite_positions("suite.txt");
    if (positions.size() == 1) {
        positions = load_suite_positions("tests/suite.txt");
    }

    int depths[] = {6, 8};
    int thread_counts[] = {1, 2, 4, 8};
    bool ok = true;

    for (int i = 0; i < static_cast<int>(positions.size()); ++i) {
        for (int depth : depths) {
            ABResult seq = best_move_alphabeta(positions[i], depth);
            for (int p : thread_counts) {
                ABParallelResult par = best_move_parallel(positions[i], depth, p);
                ok &= expect_true(
                    seq.move == par.move,
                    "pos=" + std::to_string(i + 1) + " depth=" + std::to_string(depth) +
                        " p=" + std::to_string(p) + " seq=" + std::to_string(seq.move) +
                        " par=" + std::to_string(par.move));
                ok &= expect_true(seq.value == par.value, "parallel value must match sequential value");
            }
        }
    }

    return ok;
}

bool test_parallel_loses_cross_pruning() {
    Board board;
    ABResult seq = best_move_alphabeta(board, 8);
    ABParallelResult par = best_move_parallel(board, 8, 4);

    std::cout << "[INFO Test7] nodos seq=" << seq.nodes << " par(p=4)=" << par.nodes
              << " diferencia=" << (par.nodes - seq.nodes) << "\n";
    std::cout << "[INFO Test7] podas seq=" << seq.prunes << " par(p=4)=" << par.prunes << "\n";

    bool ok = true;
    ok &= expect_true(par.nodes > seq.nodes, "parallel should explore more nodes than sequential");
    return ok;
}

bool test_slow_speedup_guarded() {
#ifdef RUN_SLOW_TESTS
    Board board;
    ABParallelResult t1 = best_move_parallel(board, 12, 1);
    ABParallelResult t4 = best_move_parallel(board, 12, 4);
    double speedup = t4.elapsed_ms > 0.0 ? t1.elapsed_ms / t4.elapsed_ms : 0.0;

    std::cout << "[INFO Test8] T(1)=" << t1.elapsed_ms << "ms"
              << " T(4)=" << t4.elapsed_ms << "ms"
              << " S(4)=" << speedup << "\n";
    return expect_true(speedup > 1.0, "4 threads should be faster than 1");
#else
    std::cout << "[INFO Test8] RUN_SLOW_TESTS no definido; test lento omitido\n";
    return true;
#endif
}

struct TestCase {
    const char* name;
    bool (*run)();
};

}  // namespace

int main() {
    std::vector<TestCase> tests = {
        {"TEST 1 - Equivalencia Minimax == Alpha-Beta", test_minimax_equals_alphabeta},
        {"TEST 2 - Turno extra respetado", test_extra_turn_respected},
        {"TEST 3 - Estado terminal positivo para P1", test_terminal_returns_extreme_value},
        {"TEST 4 - Prunes > 0", test_prunes_positive},
        {"TEST 5 - Alpha-Beta explora menos nodos", test_alphabeta_explores_fewer_nodes},
        {"TEST 6 - Equivalencia secuencial == paralelo", test_parallel_matches_sequential},
        {"TEST 7 - Paralelo explora mas nodos", test_parallel_loses_cross_pruning},
        {"TEST 8 - Speedup positivo guardado", test_slow_speedup_guarded},
    };

    int passed = 0;
    int failed = 0;
    for (const TestCase& test : tests) {
        bool ok = test.run();
        std::cout << (ok ? "PASS: " : "FAIL: ") << test.name << '\n';
        if (ok) {
            ++passed;
        } else {
            ++failed;
        }
    }

    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
