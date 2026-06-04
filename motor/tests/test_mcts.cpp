// ─────────────────────────────────────────────────────────────────────────────
// motor/tests/test_mcts.cpp
//
// Tests de MCTS — 4 pruebas requeridas por el enunciado.
//
// Framework: mini-framework propio (igual que test_alphabeta.cpp) para no
// agregar dependencias externas (Google Test / Catch2). Si en el futuro se
// quiere migrar, cada función test_* se convierte directamente en un TEST().
// ─────────────────────────────────────────────────────────────────────────────

#include "mcts.h"
#include "board.h"
#include "alphabeta.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers de aserción
// ─────────────────────────────────────────────────────────────────────────────
namespace {

bool expect_true(bool condition, const char* message) {
  if (!condition) {
    std::cout << "  FAIL: " << message << '\n';
    return false;
  }
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: construye un Board desde una línea de suite.txt
// Formato: p0,p1,p2,p3,p4,p5,K0,p6,p7,p8,p9,p10,p11,K1,side
// El campo `side` es opcional; si está presente indica el lado a mover (0 ó 1).
// Nota: en suite.txt la columna K0 es el índice 6 (almacén jugador 0)
//       y K1 es el índice 13 (almacén jugador 1).
// ─────────────────────────────────────────────────────────────────────────────
bool parse_board(const std::string& line, Board& board, int default_side = 0) {
  std::istringstream ss(line);
  std::string token;
  int idx = 0;
  board = Board{};   // reset
  while (std::getline(ss, token, ',')) {
    if (idx >= 14) {
      // Campo adicional opcional: side_to_move
      try { board.side_to_move = std::stoi(token); } catch (...) {}
      break;
    }
    try { board.pits[idx] = std::stoi(token); } catch (...) { return false; }
    ++idx;
  }
  if (idx != 14) return false;
  if (board.side_to_move == 0) board.side_to_move = default_side;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: carga las posiciones de suite.txt (ignora líneas con '#').
// ─────────────────────────────────────────────────────────────────────────────
std::vector<Board> load_suite(const std::string& path) {
  std::vector<Board> result;
  std::ifstream file(path);
  if (!file.is_open()) return result;
  std::string line;
  while (std::getline(file, line)) {
    // Quitar \r por si viene de Windows
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    Board b;
    if (parse_board(line, b, 0)) result.push_back(b);
  }
  return result;
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// TEST 1 — Movimiento siempre válido
//
// Verifica que best_move_mcts nunca retorna un índice ilegal:
//   • Para P0: debe estar en [0, 5] y el hoyo debe tener semillas.
//   • Para P1: debe estar en [7, 12] y el hoyo debe tener semillas.
//
// Se prueba desde la posición inicial y desde varias posiciones de suite.txt.
// ─────────────────────────────────────────────────────────────────────────────
bool test_move_always_valid() {
  std::cout << "[Test 1] Movimiento siempre válido\n";
  bool ok = true;

  // Posición inicial (jugador 0)
  {
    Board board;
    auto res = best_move_mcts(board, 200, 1.41421f);
    ok &= expect_true(res.move >= 0 && res.move <= 5,
                      "P0 inicio: move debe estar en [0,5]");
    ok &= expect_true(board.pits[res.move] > 0,
                      "P0 inicio: el hoyo elegido debe tener semillas");
  }

  // Posición inicial (jugador 1)
  {
    Board board;
    board.side_to_move = 1;
    // Aseguramos que el tablero sea legítimo para P1: pits[7..12] tienen semillas.
    auto res = best_move_mcts(board, 200, 1.41421f);
    ok &= expect_true(res.move >= 7 && res.move <= 12,
                      "P1 inicio: move debe estar en [7,12]");
    ok &= expect_true(board.pits[res.move] > 0,
                      "P1 inicio: el hoyo elegido debe tener semillas");
  }

  // Posiciones del suite.txt
  const auto suite = load_suite("suite.txt");
  for (int i = 0; i < static_cast<int>(suite.size()); ++i) {
    const Board& b = suite[i];
    if (is_terminal(b)) continue;
    auto res = best_move_mcts(b, 100, 1.41421f);
    const int side = b.side_to_move;
    const bool valid_range = (side == 0) ? (res.move >= 0 && res.move <= 5)
                                          : (res.move >= 7 && res.move <= 12);
    const bool has_seeds   = (res.move >= 0 && res.move < 14) && b.pits[res.move] > 0;
    char msg[128];
    std::snprintf(msg, sizeof(msg), "suite[%d]: rango valido", i);
    ok &= expect_true(valid_range, msg);
    std::snprintf(msg, sizeof(msg), "suite[%d]: hoyo con semillas", i);
    ok &= expect_true(has_seeds, msg);
  }

  std::cout << (ok ? "  PASS\n" : "  FAIL\n");
  return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 2 — Convergencia con presupuesto creciente
//
// Verifica que MCTS identifica el movimiento claramente mejor conforme aumenta
// el presupuesto de simulaciones.
//
// ── Posición diseñada (adversarialmente robusta) ─────────────────────────────
//
//   pits = {0,0,0,0,1,1, 20, 8,0,0,0,0,0, 18}, P0 to move.
//   Total: 1+1+20+8+18 = 48 ✓
//
//   Legal moves de P0: pit4 (1 semilla) y pit5 (1 semilla).
//
//   ▸ Pit 5 (CORRECTO — victoria GARANTIZADA, P1 no tiene turno en esta rama):
//       Semilla → K0 (pit6 = 21, TURNO EXTRA). P0 juega de nuevo.
//       P0 juega pit4 (1 semilla → pit5). pit5 estaba VACÍO → CAPTURA.
//       opposite(pit5) = pit7 = 8 semillas. captured = 1+8 = 9. K0 = 30.
//       P0 pits = {0,0,0,0,0,0} → terminal. K1=18.
//       Resultado: K0=30 vs K1=18. P0 GANA. Determinista. P1 nunca mueve.
//       → rollout = 1.0 siempre, incluso bajo exploración adversarial.
//
//   ▸ Pit 4 (INCORRECTO — resultado no determinista ≈ 0.50):
//       Semilla → pit5 (ahora 2 semillas). P1 a mover.
//       P1 juega pit7 (8 semillas). Distribución amplia. Juego aleatorio.
//       → rollout ≈ 0.50-0.60.
//
// Con Δ ≈ 0.45 entre ambas opciones, c=√2 y 1000 sims MCTS converge a pit5.
// ─────────────────────────────────────────────────────────────────────────────
bool test_convergence() {
  std::cout << "[Test 2] Convergencia con presupuesto creciente\n";

  Board board;
  //       p0 p1 p2 p3 p4 p5  K0   p7 p8 p9 p10 p11 p12   K1
  board.pits = {0, 0, 0, 0, 1, 1,  20,  8,  0,  0,   0,   0,   0,  18};
  board.side_to_move = 0;
  // Total: 0+0+0+0+1+1+20+8+0+0+0+0+0+18 = 48 ✓
  // pit5 (idx 5): 1 seed → K0=21 (extra turn). pit4 (1 seed → pit5). pit5 was 0
  //   → CAPTURA pit7 (8 seeds). K0=30. P0 wins. P1 never moves in this branch.
  // pit4 (idx 4): 1 seed → pit5 (2 seeds). P1 juega pit7 (8 seeds). ≈50% random.

  const int best_move_expected = 5;  // pit5: captura pit7 → victoria 30-18 garantizada

  // Con 1000 simulaciones: esperamos >70% de aciertos en N repeticiones
  {
    int hits = 0;
    const int repetitions = 20;
    for (int r = 0; r < repetitions; ++r) {
      auto res = best_move_mcts(board, 1000, 1.41421f);
      if (res.move == best_move_expected) ++hits;
    }
    const double rate = static_cast<double>(hits) / repetitions;
    std::cout << "  1000 sims: tasa = " << rate * 100 << "% (umbral 70%)\n";
    if (!expect_true(rate >= 0.70, "1000 sims: MCTS elige movimiento optimo >=70%")) {
      return false;
    }
  }

  // Con 10000 simulaciones: esperamos >90%
  {
    int hits = 0;
    const int repetitions = 20;
    for (int r = 0; r < repetitions; ++r) {
      auto res = best_move_mcts(board, 10000, 1.41421f);
      if (res.move == best_move_expected) ++hits;
    }
    const double rate = static_cast<double>(hits) / repetitions;
    std::cout << "  10000 sims: tasa = " << rate * 100 << "% (umbral 90%)\n";
    if (!expect_true(rate >= 0.90, "10000 sims: MCTS elige movimiento optimo >=90%")) {
      return false;
    }
  }

  std::cout << "  PASS\n";
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 3 — Tasa de coincidencia con AlphaBeta (Mock / Stub)
//
// Lee las posiciones de suite.txt y compara:
//   best_move_mcts(pos, 10000, √2).move
//   vs.
//   AlphaBetaEngine().SearchBestMove(pos, 8).move
//
// Reporta la tasa de coincidencia. El enunciado sólo pide REPORTAR el %,
// no superar un umbral. El test se marca PASS si suite.txt se cargó
// correctamente y la comparación se completó sin errores.
//
// ⚠ DEPENDENCIA DE P1: Si best_move_alphabeta() / AlphaBetaEngine no está
//   disponible en el proyecto, este test no compilará. En ese caso:
//   1. Comentar el bloque marcado con [INYECTAR-P1-AQUÍ].
//   2. Descomentar el stub hardcodeado que simplemente retorna PASS vacío.
// ─────────────────────────────────────────────────────────────────────────────
bool test_match_rate_vs_alphabeta() {
  std::cout << "[Test 3] Tasa de coincidencia MCTS vs AlphaBeta\n";

  // [INYECTAR-P1-AQUÍ] ── Inicio de bloque dependiente de AlphaBeta ─────────
  // Si AlphaBetaEngine no está disponible, comenta desde aquí hasta [FIN-P1].

  const auto suite = load_suite("suite.txt");
  if (suite.empty()) {
    std::cout << "  SKIP: suite.txt no encontrado o vacío\n";
    return true;  // No bloqueamos el test si no hay suite
  }

  AlphaBetaEngine ab_engine;
  const int ab_depth  = 8;
  const int mcts_sims = 10000;

  int total   = 0;
  int matches = 0;

  for (const Board& pos : suite) {
    if (is_terminal(pos)) continue;
    ++total;

    // Movimiento de AlphaBeta con profundidad 8
    const auto ab_res   = ab_engine.SearchBestMove(pos, ab_depth, 1);
    // Movimiento de MCTS con 10,000 simulaciones
    const auto mcts_res = best_move_mcts(pos, mcts_sims, 1.41421f);

    if (mcts_res.move == ab_res.move) ++matches;

    std::cout << "  pos " << total
              << ": AB=" << ab_res.move
              << " MCTS=" << mcts_res.move
              << (mcts_res.move == ab_res.move ? " ✓" : " ✗") << '\n';
  }

  const double rate = (total > 0) ? (static_cast<double>(matches) / total * 100.0) : 0.0;
  std::cout << "  Coincidencia: " << matches << "/" << total
            << " (" << rate << "%)\n";

  // El enunciado solicita REPORTAR la tasa, no impone un umbral mínimo de PASS.
  // Test PASS si se cargaron posiciones y se completó la comparación.
  const bool ok = expect_true(total > 0, "suite.txt cargado con posiciones validas");

  // [FIN-P1] ── Fin del bloque dependiente ──────────────────────────────────

  std::cout << (ok ? "  PASS\n" : "  FAIL\n");
  return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 4 — rollouts == simulations
//
// Verifica que result.rollouts sea exactamente igual al parámetro `simulations`
// pasado a best_move_mcts(). Si es menor, el ciclo está cortando antes de tiempo.
// ─────────────────────────────────────────────────────────────────────────────
bool test_rollouts_equals_simulations() {
  std::cout << "[Test 4] rollouts == simulations\n";
  bool ok = true;

  const std::vector<int> sim_budgets = {1, 10, 100, 500, 1000, 5000};

  for (int sims : sim_budgets) {
    Board board;  // Posición inicial
    auto res = best_move_mcts(board, sims, 1.41421f);
    char msg[128];
    std::snprintf(msg, sizeof(msg),
                  "rollouts (%d) == simulations (%d)", res.rollouts, sims);
    ok &= expect_true(res.rollouts == sims, msg);
  }

  // También con posición avanzada del suite
  {
    Board board;
    // Posición con pocos movimientos disponibles (endgame)
    board.pits = {0, 0, 0, 0, 0, 3, 30, 1, 2, 0, 4, 3, 0, 5};
    board.side_to_move = 0;
    auto res = best_move_mcts(board, 200, 1.41421f);
    ok &= expect_true(res.rollouts == 200,
                      "endgame: rollouts (200) == simulations (200)");
  }

  std::cout << (ok ? "  PASS\n" : "  FAIL\n");
  return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
  std::cout << "========================================\n";
  std::cout << "  Tests MCTS — Mancala/Kalah\n";
  std::cout << "========================================\n";

  int passed = 0;
  passed += test_move_always_valid()       ? 1 : 0;
  passed += test_convergence()             ? 1 : 0;
  passed += test_match_rate_vs_alphabeta() ? 1 : 0;
  passed += test_rollouts_equals_simulations() ? 1 : 0;

  std::cout << "========================================\n";
  std::cout << passed << "/4 tests passed\n";
  return passed == 4 ? 0 : 1;
}
