// ─────────────────────────────────────────────────────────────────────────────
// motor/src/mcts.cpp
//
// MCTS (Monte Carlo Tree Search) con UCT para el juego Kalah/Mancala.
//
// Implementa las 4 fases estándar:
//   1. Selección    – desciende por UCT hasta una hoja/nodo no expandido.
//   2. Expansión    – agrega exactamente UN hijo nuevo por iteración.
//   3. Simulación   – rollout aleatorio hasta estado terminal.
//   4. Retroprop.   – actualiza w y n subiendo hasta la raíz.
//
// Gestión de memoria:
//   • Los hijos se almacenan como std::unique_ptr<MCTSNode> → RAII automático.
//   • El Board dentro de cada nodo se copia una sola vez (al construir el nodo).
//   • Durante la selección se usan referencias constantes para evitar copias.
// ─────────────────────────────────────────────────────────────────────────────

#include "mcts.h"
#include "board.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Nodo del árbol MCTS
// ─────────────────────────────────────────────────────────────────────────────
struct MCTSNode {
  // ── Estado y jugada que llevó aquí ──────────────────────────────────────
  Board state;           ///< Copia del tablero en este nodo
  int   move  = -1;      ///< Índice del hoyo que generó este nodo (-1 en raíz)

  // ── Estadísticas UCT ─────────────────────────────────────────────────────
  double w    = 0.0;     ///< Victorias acumuladas (perspectiva del jugador raíz)
  int    n    = 0;       ///< Número de visitas

  // ── Relaciones en el árbol ───────────────────────────────────────────────
  MCTSNode* parent = nullptr;                          ///< Puntero al padre (raw, NO owning)
  std::vector<std::unique_ptr<MCTSNode>> children;    ///< Hijos (owning, RAII)

  // ── Movimientos pendientes de explorar ──────────────────────────────────
  std::vector<int> untried_moves;  ///< Movimientos legales aún sin expandir

  // ── Constructor ─────────────────────────────────────────────────────────
  explicit MCTSNode(const Board& s, int mv, MCTSNode* par)
      : state(s), move(mv), parent(par) {
    // Precalculamos los movimientos legales para expansion lazy.
    untried_moves = legal_moves(state);
  }

  // Comprueba si todos los hijos legales han sido expandidos.
  bool fully_expanded() const { return untried_moves.empty(); }

  // Comprueba si es un nodo terminal (ningún movimiento legal posible).
  bool is_terminal() const {
    return ::is_terminal(state);
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Sección: Funciones auxiliares
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// Calcula el valor UCT de un nodo hijo.
//   child   – nodo hijo a evaluar
//   c       – constante de exploración
//   pov     – jugador de la raíz
//
// INVARIANTE: child.w siempre acumula victorias desde la perspectiva de `pov`
// (sin inversión en retroprop). Por tanto, cuando el PADRE es el oponente, él
// quiere MINIMIZAR w/n de pov ↔ MAXIMIZAR (1 - w/n). Ajustamos `exploit`
// según quién es el padre, no quién es el hijo.
double uct_value(const MCTSNode& child, double c, int pov) {
  // Si el hijo no ha sido visitado → infinito para forzar su exploración.
  if (child.n == 0) return std::numeric_limits<double>::infinity();

  assert(child.parent != nullptr);
  const double N_parent = static_cast<double>(child.parent->n);
  const double explore  = c * std::sqrt(std::log(N_parent) / static_cast<double>(child.n));

  // El padre elige el movimiento:
  //   • Si es el turno del jugador raíz (pov) → maximiza victorias de pov → exploit = w/n
  //   • Si es el turno del oponente           → maximiza victorias del oponente
  //                                             = minimiza victorias de pov
  //                                             → exploit = 1 - w/n
  //
  // Nota: child.parent->state.side_to_move es quien está eligiendo el movimiento
  // que lleva a `child`, es decir, el decisor en el nodo padre.
  double exploit;
  if (child.parent->state.side_to_move == pov) {
    exploit = child.w / static_cast<double>(child.n);
  } else {
    exploit = 1.0 - child.w / static_cast<double>(child.n);
  }

  return exploit + explore;
}

// ─────────────────────────────────────────────────────────────────────────────
// FASE 1: Selección
// Desciende desde `node` eligiendo el hijo con mayor UCT hasta llegar a un
// nodo que no esté completamente expandido o que sea terminal.
// ─────────────────────────────────────────────────────────────────────────────
MCTSNode* select(MCTSNode* node, double c, int pov) {
  while (!node->is_terminal() && node->fully_expanded()) {
    // Elige el hijo con mayor valor UCT.
    MCTSNode* best_child = nullptr;
    double    best_uct   = -std::numeric_limits<double>::infinity();

    for (auto& child_ptr : node->children) {
      const double val = uct_value(*child_ptr, c, pov);
      if (val > best_uct) {
        best_uct   = val;
        best_child = child_ptr.get();
      }
    }

    assert(best_child != nullptr);
    node = best_child;
  }
  return node;
}

// ─────────────────────────────────────────────────────────────────────────────
// FASE 2: Expansión
// Agrega exactamente UN hijo no explorado al nodo seleccionado.
// Retorna el nuevo nodo (o el mismo si ya es terminal).
// ─────────────────────────────────────────────────────────────────────────────
MCTSNode* expand(MCTSNode* node, std::mt19937& rng) {
  if (node->is_terminal() || node->untried_moves.empty()) return node;

  // Tomamos un movimiento al azar de los no explorados (shuffle lazy).
  const int idx = static_cast<int>(
      std::uniform_int_distribution<std::size_t>(0, node->untried_moves.size() - 1)(rng));

  const int chosen_move = node->untried_moves[idx];

  // Eliminamos el movimiento de la lista de no explorados (swap-and-pop → O(1)).
  node->untried_moves[idx] = node->untried_moves.back();
  node->untried_moves.pop_back();

  // Aplicamos el movimiento para obtener el estado hijo.
  bool  extra_turn = false;
  Board child_state = apply_move(node->state, chosen_move, extra_turn);
  // Nota: apply_move ya actualiza side_to_move (si no hay turno extra, cambia al oponente).

  // Creamos el nuevo nodo y lo insertamos como hijo.
  auto new_node = std::make_unique<MCTSNode>(child_state, chosen_move, node);
  MCTSNode* result = new_node.get();
  node->children.push_back(std::move(new_node));

  return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// FASE 3: Simulación (rollout)
// Desde `start_state` juega movimientos aleatorios hasta estado terminal.
// Retorna:
//   1.0  → gana el jugador raíz (pov)
//   0.0  → pierde el jugador raíz
//   0.5  → empate
// También incrementa `depth_accumulator` con la profundidad del rollout.
// ─────────────────────────────────────────────────────────────────────────────
double rollout(const Board& start_state, int pov, std::mt19937& rng, int& out_depth) {
  Board current = start_state;  // Una sola copia al inicio del rollout
  int   depth   = 0;

  while (!::is_terminal(current)) {
    const auto moves = legal_moves(current);
    if (moves.empty()) break;

    // Movimiento aleatorio uniforme.
    const int chosen = moves[
        std::uniform_int_distribution<int>(0, static_cast<int>(moves.size()) - 1)(rng)];

    bool extra_turn = false;
    // apply_move devuelve una copia; la asignamos sobre `current` (sin alloc extra).
    current = apply_move(current, chosen, extra_turn);
    ++depth;
  }

  out_depth = depth;

  // Determinar ganador desde la perspectiva del jugador raíz.
  const int w = winner(current);
  if (w == pov)  return 1.0;
  if (w == -1)   return 0.5;   // Empate
  return 0.0;                   // Ganó el oponente
}

// ─────────────────────────────────────────────────────────────────────────────
// FASE 4: Retropropagación
// Sube desde `node` hasta la raíz actualizando n y w.
//
// INVARIANTE: w siempre acumula victorias desde la perspectiva de `pov`
// (el jugador de la raíz). NO se invierte por nivel. La inversión de perspectiva
// ocurre en uct_value según quién es el padre que toma la decisión.
void backpropagate(MCTSNode* node, double result, int /*pov*/) {
  while (node != nullptr) {
    ++node->n;
    // Acumulamos directamente desde la perspectiva del jugador raíz.
    // uct_value se encarga de invertir cuando el padre es el oponente.
    node->w += result;
    node = node->parent;
  }
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Función principal: best_move_mcts
// ─────────────────────────────────────────────────────────────────────────────
MCTSResult best_move_mcts(const Board& board, int simulations, float c) {
  // Jugador que inicia la búsqueda (perspectiva de la raíz).
  const int pov = board.side_to_move;

  // RNG con semilla aleatoria (no determinista entre ejecuciones).
  std::mt19937 rng(std::random_device{}());

  // Crear nodo raíz (copia del board de partida).
  auto root = std::make_unique<MCTSNode>(board, -1, nullptr);
  // La raíz necesita n=1 para que UCT del primer nivel no divida entre 0.
  // Sin embargo, el estándar de MCTS inicializa n=0 y selecciona siempre
  // hijos no visitados primero (UCT = ∞); lo dejamos en 0 y la lógica de uct_value
  // devuelve ∞ para nodos con n==0, lo que fuerza su exploración.

  // Acumuladores para estadísticas de profundidad.
  long long total_depth = 0;

  // ── Ciclo principal MCTS ──────────────────────────────────────────────────
  for (int sim = 0; sim < simulations; ++sim) {
    // FASE 1: Selección
    MCTSNode* node = select(root.get(), static_cast<double>(c), pov);

    // FASE 2: Expansión (solo si no es terminal)
    if (!node->is_terminal()) {
      node = expand(node, rng);
    }

    // FASE 3: Simulación (rollout aleatorio desde el nuevo nodo)
    int rollout_depth = 0;
    const double result = rollout(node->state, pov, rng, rollout_depth);
    total_depth += rollout_depth;

    // FASE 4: Retropropagación
    backpropagate(node, result, pov);
  }

  // ── Elegir el hijo de la raíz con mayor número de visitas ────────────────
  // (Mayor n es más robusto que mayor w/n para la decisión final.)
  MCTSResult out;
  out.rollouts       = simulations;
  out.tree_depth_avg = simulations > 0
                           ? static_cast<double>(total_depth) / simulations
                           : 0.0;

  int    best_n    = -1;
  double best_rate = 0.0;
  for (const auto& child : root->children) {
    if (child->n > best_n) {
      best_n    = child->n;
      out.move  = child->move;
      best_rate = child->n > 0 ? child->w / child->n : 0.0;
    }
  }
  out.win_rate = best_rate;

  // Fallback: si por alguna razón no hay hijos (posición inicial terminal),
  // retornamos el primer movimiento legal disponible.
  if (out.move == -1) {
    const auto moves = legal_moves(board);
    if (!moves.empty()) out.move = moves[0];
  }

  return out;
}
