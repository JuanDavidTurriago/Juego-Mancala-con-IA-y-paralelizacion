#include "alphabeta.h"
#include "board.h"
#include "httplib.h"
#include "json.hpp"
#include "mcts.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include <omp.h>

using json = nlohmann::json;

namespace {

constexpr int kServerPort = 8080;
constexpr float kMctsExploration = 1.41421f;

std::atomic<bool> g_ready{false};
std::atomic<std::int64_t> g_http_requests{0};
std::atomic<std::int64_t> g_move_requests{0};
std::atomic<std::int64_t> g_total_nodes{0};
std::atomic<std::int64_t> g_total_prunes{0};
std::atomic<std::int64_t> g_total_rollouts{0};
std::atomic<std::int64_t> g_total_elapsed_ms{0};

struct MoveRequest {
  Board board;
  std::string algo = "alphabeta";
  int depth = 8;
  int simulations = 1000;
  int threads = 1;
};

void write_json(httplib::Response& res, int status, const json& body) {
  res.status = status;
  res.set_content(body.dump(), "application/json; charset=utf-8");
}

bool is_integer_array(const json& value) {
  if (!value.is_array()) {
    return false;
  }
  return std::all_of(value.begin(), value.end(), [](const json& item) {
    return item.is_number_integer();
  });
}

bool read_int_field(const json& body, const char* name, int default_value, int& out) {
  if (!body.contains(name)) {
    out = default_value;
    return true;
  }
  if (!body[name].is_number_integer()) {
    return false;
  }
  out = body[name].get<int>();
  return true;
}

bool read_int_field_any(const json& body, const json& params, const char* canonical_name,
                        const char* legacy_name, int default_value, int& out) {
  if (params.contains(canonical_name)) {
    if (!params[canonical_name].is_number_integer()) {
      return false;
    }
    out = params[canonical_name].get<int>();
    return true;
  }
  if (body.contains(canonical_name)) {
    if (!body[canonical_name].is_number_integer()) {
      return false;
    }
    out = body[canonical_name].get<int>();
    return true;
  }
  return read_int_field(body, legacy_name, default_value, out);
}

bool read_string_field(const json& body, const char* name, const char* default_value,
                       std::string& out) {
  if (!body.contains(name)) {
    out = default_value;
    return true;
  }
  if (!body[name].is_string()) {
    return false;
  }
  out = body[name].get<std::string>();
  return true;
}

bool load_board(const json& body, Board& board) {
  const json* board_value = nullptr;
  if (body.contains("tablero")) {
    board_value = &body["tablero"];
  } else if (body.contains("board")) {
    board_value = &body["board"];
  }

  if (board_value == nullptr || !is_integer_array(*board_value) ||
      board_value->size() != kBoardSize) {
    return false;
  }

  for (int i = 0; i < kBoardSize; ++i) {
    const int seeds = (*board_value)[i].get<int>();
    if (seeds < 0) {
      return false;
    }
    board.pits[i] = seeds;
  }

  return total_seed_count(board) == kInitialSeedTotal;
}

bool parse_move_request(const json& body, MoveRequest& out, std::string& error) {
  if (!load_board(body, out.board)) {
    error = "invalid board";
    return false;
  }

  const char* side_field = body.contains("lado") ? "lado" : "side";
  if (!body.contains(side_field) || !body[side_field].is_number_integer()) {
    error = "invalid side";
    return false;
  }
  out.board.side_to_move = body[side_field].get<int>();
  if (!is_valid_side(out.board.side_to_move)) {
    error = "invalid side";
    return false;
  }

  const char* algo_field = body.contains("algoritmo") ? "algoritmo" : "algo";
  if (!read_string_field(body, algo_field, "alphabeta", out.algo)) {
    error = "unknown algo";
    return false;
  }
  if (out.algo == "alphabet") {
    out.algo = "alphabeta";
  }
  if (out.algo != "alphabeta" && out.algo != "mcts") {
    error = "unknown algo";
    return false;
  }

  json empty_params = json::object();
  const json& params = body.contains("parametros") ? body["parametros"] : empty_params;
  if (!params.is_object()) {
    error = "invalid parametros";
    return false;
  }

  if (!read_int_field_any(body, params, "hilos", "threads", 1, out.threads)) {
    error = "invalid threads";
    return false;
  }
  if (out.threads < 1 || out.threads > 64) {
    error = "invalid threads";
    return false;
  }

  if (out.algo == "alphabeta") {
    if (!read_int_field_any(body, params, "profundidad", "depth", 8, out.depth)) {
      error = "invalid depth";
      return false;
    }
    if (out.depth < 1 || out.depth > 32) {
      error = "invalid depth";
      return false;
    }
  } else {
    if (!read_int_field_any(body, params, "simulaciones", "simulations", 1000, out.simulations)) {
      error = "invalid simulations";
      return false;
    }
    if (out.simulations < 1 || out.simulations > 100000) {
      error = "invalid simulations";
      return false;
    }
  }

  return true;
}

json alphabeta_response(const MoveRequest& request) {
  const double t0 = omp_get_wtime();
  const ABParallelResult result =
      best_move_parallel(request.board, request.depth, request.threads);
  const double elapsed_ms = (omp_get_wtime() - t0) * 1000.0;

  g_move_requests.fetch_add(1);
  g_total_nodes.fetch_add(result.nodes);
  g_total_prunes.fetch_add(result.prunes);
  g_total_elapsed_ms.fetch_add(static_cast<std::int64_t>(elapsed_ms));

  json stats = json{
      {"algo", "alphabeta"},
      {"nodes", result.nodes},
      {"prunes", result.prunes},
  };
  json estadisticas = json{
      {"algoritmo", "alphabeta"},
      {"profundidad", request.depth},
      {"nodos", result.nodes},
      {"podas", result.prunes},
      {"tiempo_ms", static_cast<int>(elapsed_ms)},
      {"hilos", result.threads_used},
      {"evaluacion", result.value},
  };

  return json{
      {"algo", "alphabeta"},
      {"move", result.move},
      {"movimiento", result.move},
      {"evaluation", result.value},
      {"elapsed_ms", static_cast<int>(elapsed_ms)},
      {"stats", stats},
      {"estadisticas", estadisticas},
      {"threads_used", result.threads_used},
  };
}

json mcts_response(const MoveRequest& request) {
  const double t0 = omp_get_wtime();
  const MCTSResult result =
      best_move_mcts(request.board, request.simulations, kMctsExploration);
  const double elapsed_ms = (omp_get_wtime() - t0) * 1000.0;
  const int evaluation = static_cast<int>((result.win_rate - 0.5) * 200.0);

  g_move_requests.fetch_add(1);
  g_total_rollouts.fetch_add(result.rollouts);
  g_total_elapsed_ms.fetch_add(static_cast<std::int64_t>(elapsed_ms));

  json stats = json{
      {"algo", "mcts"},
      {"rollouts", result.rollouts},
      {"win_rate", result.win_rate},
      {"tree_depth_avg", result.tree_depth_avg},
  };
  json estadisticas = json{
      {"algoritmo", "mcts"},
      {"simulaciones", request.simulations},
      {"rollouts", result.rollouts},
      {"win_rate", result.win_rate},
      {"tiempo_ms", static_cast<int>(elapsed_ms)},
      {"hilos", 1},
      {"evaluacion", evaluation},
  };

  return json{
      {"algo", "mcts"},
      {"move", result.move},
      {"movimiento", result.move},
      {"evaluation", evaluation},
      {"elapsed_ms", static_cast<int>(elapsed_ms)},
      {"stats", stats},
      {"estadisticas", estadisticas},
      {"threads_used", 1},
  };
}

std::string metrics_body() {
  return
      "# HELP mancala_motor_http_requests_total HTTP requests handled by the motor\n"
      "# TYPE mancala_motor_http_requests_total counter\n"
      "mancala_motor_http_requests_total " + std::to_string(g_http_requests.load()) + "\n"
      "# HELP mancala_motor_move_requests_total Move requests handled by the motor\n"
      "# TYPE mancala_motor_move_requests_total counter\n"
      "mancala_motor_move_requests_total " + std::to_string(g_move_requests.load()) + "\n"
      "# HELP mancala_motor_search_nodes_total Alpha-Beta nodes searched\n"
      "# TYPE mancala_motor_search_nodes_total counter\n"
      "mancala_motor_search_nodes_total " + std::to_string(g_total_nodes.load()) + "\n"
      "# HELP mancala_motor_search_prunes_total Alpha-Beta pruning cuts\n"
      "# TYPE mancala_motor_search_prunes_total counter\n"
      "mancala_motor_search_prunes_total " + std::to_string(g_total_prunes.load()) + "\n"
      "# HELP mancala_motor_mcts_rollouts_total MCTS rollouts completed\n"
      "# TYPE mancala_motor_mcts_rollouts_total counter\n"
      "mancala_motor_mcts_rollouts_total " + std::to_string(g_total_rollouts.load()) + "\n"
      "# HELP mancala_motor_search_elapsed_ms_total Search time in milliseconds\n"
      "# TYPE mancala_motor_search_elapsed_ms_total counter\n"
      "mancala_motor_search_elapsed_ms_total " + std::to_string(g_total_elapsed_ms.load()) + "\n";
}

}  // namespace

int main() {
  httplib::Server server;

  server.Get("/healthz", [](const httplib::Request&, httplib::Response& res) {
    g_http_requests.fetch_add(1);
    write_json(res, 200, json{{"status", "ok"}});
  });

  server.Get("/readyz", [](const httplib::Request&, httplib::Response& res) {
    g_http_requests.fetch_add(1);
    if (g_ready.load()) {
      write_json(res, 200, json{{"status", "ready"}});
    } else {
      write_json(res, 503, json{{"status", "starting"}});
    }
  });

  server.Get("/metrics", [](const httplib::Request&, httplib::Response& res) {
    g_http_requests.fetch_add(1);
    res.status = 200;
    res.set_content(metrics_body(), "text/plain; version=0.0.4; charset=utf-8");
  });

  server.Post("/move", [](const httplib::Request& req, httplib::Response& res) {
    g_http_requests.fetch_add(1);

    try {
      const json body = json::parse(req.body);
      MoveRequest move_request;
      std::string error;
      if (!parse_move_request(body, move_request, error)) {
        write_json(res, 400, json{{"error", error}});
        return;
      }

      const json response = move_request.algo == "mcts"
                                ? mcts_response(move_request)
                                : alphabeta_response(move_request);
      write_json(res, 200, response);
    } catch (const json::parse_error&) {
      write_json(res, 400, json{{"error", "invalid json"}});
    } catch (const std::exception& ex) {
      write_json(res, 500, json{{"error", "internal error"}, {"detail", ex.what()}});
    } catch (...) {
      write_json(res, 500, json{{"error", "internal error"}});
    }
  });

  g_ready.store(true);
  std::cerr << "motor_server listening on 0.0.0.0:" << kServerPort << '\n';
  if (!server.listen("0.0.0.0", kServerPort)) {
    g_ready.store(false);
    std::cerr << "motor_server failed to listen on port " << kServerPort << '\n';
    return 1;
  }

  return 0;
}
