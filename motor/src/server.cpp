#include "alphabeta.h"
#include "board.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct MoveRequest {
  Board board;
  int depth = 8;
  int threads = 1;
};

struct HttpRequest {
  std::string method;
  std::string path;
  std::string body;
};

std::atomic<std::int64_t> g_http_requests{0};
std::atomic<std::int64_t> g_move_requests{0};
std::atomic<std::int64_t> g_total_nodes{0};
std::atomic<std::int64_t> g_total_prunes{0};
std::atomic<std::int64_t> g_total_elapsed_ms{0};

std::string trim_copy(const std::string& value) {
  size_t begin = 0;
  while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
    ++begin;
  }
  size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(begin, end - begin);
}

std::optional<int> parse_int_at(const std::string& text, size_t& pos) {
  while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
    ++pos;
  }
  if (pos >= text.size()) {
    return std::nullopt;
  }

  char* end_ptr = nullptr;
  const long value = std::strtol(text.c_str() + pos, &end_ptr, 10);
  if (end_ptr == text.c_str() + pos) {
    return std::nullopt;
  }
  pos = static_cast<size_t>(end_ptr - text.c_str());
  return static_cast<int>(value);
}

std::optional<size_t> find_field_value(const std::string& json, const std::string& field) {
  const std::string key = "\"" + field + "\"";
  const size_t key_pos = json.find(key);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const size_t colon = json.find(':', key_pos + key.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  return colon + 1;
}

std::optional<int> parse_int_field(const std::string& json, const std::string& field) {
  auto pos = find_field_value(json, field);
  if (!pos) {
    return std::nullopt;
  }
  return parse_int_at(json, *pos);
}

std::optional<std::vector<int>> parse_board_field(const std::string& json) {
  auto pos = find_field_value(json, "board");
  if (!pos) {
    return std::nullopt;
  }
  size_t cursor = *pos;
  while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
    ++cursor;
  }
  if (cursor >= json.size() || json[cursor] != '[') {
    return std::nullopt;
  }
  ++cursor;

  std::vector<int> values;
  while (cursor < json.size()) {
    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
      ++cursor;
    }
    if (cursor < json.size() && json[cursor] == ']') {
      return values;
    }

    auto number = parse_int_at(json, cursor);
    if (!number) {
      return std::nullopt;
    }
    values.push_back(*number);

    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
      ++cursor;
    }
    if (cursor < json.size() && json[cursor] == ',') {
      ++cursor;
      continue;
    }
    if (cursor < json.size() && json[cursor] == ']') {
      return values;
    }
    return std::nullopt;
  }
  return std::nullopt;
}

std::optional<MoveRequest> parse_move_request(const std::string& json, std::string& error) {
  const auto board_values = parse_board_field(json);
  const auto side = parse_int_field(json, "side");
  const auto depth = parse_int_field(json, "depth");
  const auto threads = parse_int_field(json, "threads");

  if (!board_values || !side || !depth || !threads) {
    error = "expected fields: board, side, depth, threads";
    return std::nullopt;
  }
  if (board_values->size() != kBoardSize) {
    error = "board must contain exactly 14 positions";
    return std::nullopt;
  }
  if (!is_valid_side(*side)) {
    error = "side must be 0 or 1";
    return std::nullopt;
  }
  if (*depth < 1 || *depth > 32) {
    error = "depth must be between 1 and 32";
    return std::nullopt;
  }
  if (*threads < 1 || *threads > 64) {
    error = "threads must be between 1 and 64";
    return std::nullopt;
  }

  MoveRequest req;
  req.board.side_to_move = *side;
  for (int i = 0; i < kBoardSize; ++i) {
    if ((*board_values)[i] < 0) {
      error = "board positions must be non-negative";
      return std::nullopt;
    }
    req.board.pits[i] = (*board_values)[i];
  }
  if (total_seed_count(req.board) != kInitialSeedTotal) {
    error = "board seed total must be 48";
    return std::nullopt;
  }
  req.depth = *depth;
  req.threads = *threads;
  return req;
}

std::string reason_phrase(int status) {
  switch (status) {
    case 200:
      return "OK";
    case 400:
      return "Bad Request";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    default:
      return "Internal Server Error";
  }
}

std::string http_response(int status, const std::string& body, const std::string& content_type) {
  std::ostringstream out;
  out << "HTTP/1.1 " << status << ' ' << reason_phrase(status) << "\r\n"
      << "Content-Type: " << content_type << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "\r\n"
      << body;
  return out.str();
}

std::string json_response(int status, const std::string& body) {
  return http_response(status, body, "application/json; charset=utf-8");
}

std::string error_response(int status, const std::string& detail) {
  return json_response(status, "{\"detail\":\"" + detail + "\"}");
}

std::string metrics_response() {
  std::ostringstream body;
  body << "# HELP mancala_motor_http_requests_total HTTP requests handled by the motor\n"
       << "# TYPE mancala_motor_http_requests_total counter\n"
       << "mancala_motor_http_requests_total " << g_http_requests.load() << "\n"
       << "# HELP mancala_motor_move_requests_total Move requests handled by the motor\n"
       << "# TYPE mancala_motor_move_requests_total counter\n"
       << "mancala_motor_move_requests_total " << g_move_requests.load() << "\n"
       << "# HELP mancala_motor_search_nodes_total Alpha-Beta nodes searched\n"
       << "# TYPE mancala_motor_search_nodes_total counter\n"
       << "mancala_motor_search_nodes_total " << g_total_nodes.load() << "\n"
       << "# HELP mancala_motor_search_prunes_total Alpha-Beta pruning cuts\n"
       << "# TYPE mancala_motor_search_prunes_total counter\n"
       << "mancala_motor_search_prunes_total " << g_total_prunes.load() << "\n"
       << "# HELP mancala_motor_search_elapsed_ms_total Search time in milliseconds\n"
       << "# TYPE mancala_motor_search_elapsed_ms_total counter\n"
       << "mancala_motor_search_elapsed_ms_total " << g_total_elapsed_ms.load() << "\n";
  return http_response(200, body.str(), "text/plain; version=0.0.4; charset=utf-8");
}

std::optional<int> content_length_from_headers(const std::string& headers) {
  std::istringstream in(headers);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    const size_t colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    std::string name = line.substr(0, colon);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    if (trim_copy(name) == "content-length") {
      size_t pos = colon + 1;
      return parse_int_at(line, pos);
    }
  }
  return 0;
}

std::optional<HttpRequest> read_http_request(int client_fd) {
  std::string raw;
  char buffer[4096];
  size_t header_end = std::string::npos;

  while ((header_end = raw.find("\r\n\r\n")) == std::string::npos) {
    const ssize_t bytes = read(client_fd, buffer, sizeof(buffer));
    if (bytes <= 0) {
      return std::nullopt;
    }
    raw.append(buffer, static_cast<size_t>(bytes));
    if (raw.size() > 1024 * 1024) {
      return std::nullopt;
    }
  }

  const std::string headers = raw.substr(0, header_end + 2);
  const int content_length = content_length_from_headers(headers).value_or(0);
  if (content_length < 0) {
    return std::nullopt;
  }
  const size_t body_start = header_end + 4;
  while (raw.size() < body_start + static_cast<size_t>(content_length)) {
    const ssize_t bytes = read(client_fd, buffer, sizeof(buffer));
    if (bytes <= 0) {
      return std::nullopt;
    }
    raw.append(buffer, static_cast<size_t>(bytes));
  }

  std::istringstream request_line(headers);
  HttpRequest req;
  request_line >> req.method >> req.path;
  req.body = raw.substr(body_start, static_cast<size_t>(content_length));
  return req;
}

std::string handle_move(const std::string& body) {
  std::string error;
  auto request = parse_move_request(body, error);
  if (!request) {
    return error_response(400, error);
  }

  const auto started = std::chrono::steady_clock::now();
  AlphaBetaEngine engine;
  const AlphaBetaResult result =
      engine.SearchBestMove(request->board, request->depth, request->threads);
  const auto ended = std::chrono::steady_clock::now();
  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(ended - started).count();

  g_move_requests.fetch_add(1);
  g_total_nodes.fetch_add(result.stats.nodes);
  g_total_prunes.fetch_add(result.stats.prunes);
  g_total_elapsed_ms.fetch_add(elapsed_ms);

  std::ostringstream json;
  json << "{\"move\":" << result.move << ","
       << "\"evaluation\":" << result.evaluation << ","
       << "\"elapsed_ms\":" << elapsed_ms << ","
       << "\"stats\":{\"nodes\":" << result.stats.nodes << ","
       << "\"prunes\":" << result.stats.prunes << "},"
       << "\"threads_used\":" << result.threads_used << "}";
  return json_response(200, json.str());
}

std::string route_request(const HttpRequest& request) {
  g_http_requests.fetch_add(1);

  if (request.method == "GET" && request.path == "/healthz") {
    return json_response(200, "{\"status\":\"ok\"}");
  }
  if (request.method == "GET" && request.path == "/readyz") {
    return json_response(200, "{\"status\":\"ready\"}");
  }
  if (request.method == "GET" && request.path == "/metrics") {
    return metrics_response();
  }
  if (request.path == "/move" && request.method != "POST") {
    return error_response(405, "POST required for /move");
  }
  if (request.method == "POST" && request.path == "/move") {
    return handle_move(request.body);
  }
  return error_response(404, "not found");
}

void handle_client(int client_fd) {
  const auto request = read_http_request(client_fd);
  const std::string response =
      request ? route_request(*request) : error_response(400, "malformed HTTP request");
  if (write(client_fd, response.c_str(), response.size()) < 0) {
    std::cerr << "write failed: " << std::strerror(errno) << '\n';
  }
  close(client_fd);
}

int configured_port() {
  const char* env_port = std::getenv("MOTOR_PORT");
  if (!env_port) {
    return 9000;
  }
  try {
    return std::stoi(env_port);
  } catch (...) {
    return 9000;
  }
}

}  // namespace

int main() {
  const int port = configured_port();
  const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "socket failed: " << std::strerror(errno) << '\n';
    return 1;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(port));

  if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "bind failed: " << std::strerror(errno) << '\n';
    close(server_fd);
    return 1;
  }

  if (listen(server_fd, 64) < 0) {
    std::cerr << "listen failed: " << std::strerror(errno) << '\n';
    close(server_fd);
    return 1;
  }

  std::cerr << "mancala_engine listening on 0.0.0.0:" << port << '\n';
  while (true) {
    const int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
      std::cerr << "accept failed: " << std::strerror(errno) << '\n';
      continue;
    }
    handle_client(client_fd);
  }
}
