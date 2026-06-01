#include "mcts.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <vector>

// ============================================================================
// TAREA 1: MCTSNode structure + memory management
// ============================================================================
namespace {

struct MCTSNode {
  Board board;
  int move = -1;
  int n = 0;
  double v = 0.0;
  std::vector<std::unique_ptr<MCTSNode>> children;
  MCTSNode* parent = nullptr;

  MCTSNode() = default;
  MCTSNode(const Board& b, int m) : board(b), move(m) {}
};

// ============================================================================
// Random number generation (thread-local for future parallelization)
// ============================================================================
thread_local std::mt19937 g_rng{std::random_device{}()};

// ============================================================================
// TAREA 2, FASE 1: Selection
// Select child with highest UCT value until reaching a leaf or terminal.
// UCT(n) = w/n + c * sqrt(ln(n_parent) / n)
// ============================================================================
MCTSNode* SelectionPhase(MCTSNode* root, double exploration_c) {
  MCTSNode* current = root;
  while (!current->children.empty() && !is_terminal(current->board)) {
    // Find child with maximum UCT value
    MCTSNode* best_child = nullptr;
    double best_uct = -1.0;

    for (auto& child : current->children) {
      if (child->n == 0) {
        // Unvisited children are never selected (already handled in Expansion)
        continue;
      }
      double win_rate = child->v / child->n;
      double exploration =
          exploration_c * std::sqrt(std::log(current->n) / child->n);
      double uct = win_rate + exploration;

      if (uct > best_uct) {
        best_uct = uct;
        best_child = child.get();
      }
    }

    if (best_child == nullptr) {
      // All children are unvisited or terminal reached
      break;
    }
    current = best_child;
  }
  return current;
}

// ============================================================================
// TAREA 2, FASE 2: Expansion
// Add one new child to the current node if it has unexplored moves.
// ============================================================================
MCTSNode* ExpansionPhase(MCTSNode* current) {
  if (is_terminal(current->board)) {
    return current;
  }

  auto moves = legal_moves(current->board);
  // Find a move that hasn't been explored yet
  for (int move : moves) {
    bool move_exists = false;
    for (auto& child : current->children) {
      if (child->move == move) {
        move_exists = true;
        break;
      }
    }

    if (!move_exists) {
      // Create new child for this move
      bool extra_turn = false;
      Board child_board = apply_move(current->board, move, extra_turn);

      // If extra_turn, the active player stays the same; otherwise switch.
      if (!extra_turn) {
        child_board.side_to_move = 1 - child_board.side_to_move;
      }

      auto new_node = std::make_unique<MCTSNode>(child_board, move);
      new_node->parent = current;
      MCTSNode* new_node_ptr = new_node.get();
      current->children.push_back(std::move(new_node));
      return new_node_ptr;
    }
  }
  // All moves have been explored; return current to avoid errors
  return current;
}

// ============================================================================
// TAREA 2, FASE 3: Simulation (Rollout)
// Play out the game from the current node to terminal, choosing random moves.
// Returns: 1.0 if root player wins, 0.0 if loses, 0.5 if draw.
// Also returns the depth of the simulation (for tree_depth_avg calculation).
// ============================================================================
std::pair<double, int> SimulationPhase(const Board& start_board,
                                       int root_side_to_move) {
  Board sim_board = start_board;
  int depth = 0;

  while (!is_terminal(sim_board)) {
    auto moves = legal_moves(sim_board);
    if (moves.empty()) break;

    // Choose random move
    std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
    int move = moves[dist(g_rng)];

    bool extra_turn = false;
    sim_board = apply_move(sim_board, move, extra_turn);

    if (!extra_turn) {
      sim_board.side_to_move = 1 - sim_board.side_to_move;
    }

    depth++;
  }

  // Determine outcome from root player perspective
  int w = winner(sim_board);
  double outcome = 0.5;
  if (w == root_side_to_move) {
    outcome = 1.0;
  } else if (w != -1) {
    outcome = 0.0;
  }

  return {outcome, depth};
}

// ============================================================================
// TAREA 2, FASE 4: Backpropagation
// Update n and v from the expanded node up to the root.
// ============================================================================
void BackpropagationPhase(MCTSNode* node, double outcome) {
  MCTSNode* current = node;
  while (current != nullptr) {
    current->n++;
    current->v += outcome;
    current = current->parent;
  }
}

// ============================================================================
// Compute average tree depth from all simulations
// We'll track this during search and compute the average.
// ============================================================================
struct SearchStats {
  std::int64_t total_rollouts = 0;
  double total_depth = 0.0;
};

}  // namespace

// ============================================================================
// TAREA 3: best_move_mcts implementation
// ============================================================================
MctsEngine::MctsEngine(double exploration_c) : exploration_c_(exploration_c) {}

MctsResult MctsEngine::SearchBestMove(const Board& board, int simulations) {
  // Create root node
  auto root = std::make_unique<MCTSNode>(board, -1);
  MCTSNode* root_ptr = root.get();
  int root_side_to_move = board.side_to_move;

  SearchStats stats;

  // Run MCTS cycle: repeat all 4 phases `simulations` times
  for (int i = 0; i < simulations; i++) {
    // Phase 1: Selection
    MCTSNode* leaf = SelectionPhase(root_ptr, exploration_c_);

    // Phase 2: Expansion (returns a new node if possible, else current)
    MCTSNode* to_simulate = ExpansionPhase(leaf);

    // Phase 3: Simulation from the expanded node
    auto [outcome, depth] = SimulationPhase(to_simulate->board, root_side_to_move);
    stats.total_depth += depth;
    stats.total_rollouts++;

    // Phase 4: Backpropagation
    BackpropagationPhase(to_simulate, outcome);
  }

  // After all simulations, select best move (most visits, not highest win rate)
  MctsResult result;
  result.move = -1;
  result.evaluation = 0.0;
  result.stats.rollouts = simulations;
  result.stats.win_rate = 0.0;
  result.stats.tree_depth_avg =
      stats.total_rollouts > 0 ? stats.total_depth / stats.total_rollouts : 0.0;

  int best_n = -1;
  for (auto& child : root_ptr->children) {
    if (child->n > best_n) {
      best_n = child->n;
      result.move = child->move;
      result.evaluation = child->v / child->n;
      result.stats.win_rate = child->v / child->n;
    }
  }

  return result;
}
