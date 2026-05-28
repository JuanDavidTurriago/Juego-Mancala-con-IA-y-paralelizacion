#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

namespace {

constexpr int kPort = 8080;
constexpr char kHealthBody[] = "Motor pendiente";
constexpr size_t kHealthBodyLen = sizeof(kHealthBody) - 1;

std::string make_health_response() {
  return "HTTP/1.1 200 OK\r\n"
         "Content-Type: text/plain; charset=utf-8\r\n"
         "Content-Length: " +
         std::to_string(kHealthBodyLen) +
         "\r\n"
         "Connection: close\r\n"
         "\r\n" +
         kHealthBody;
}

std::string make_not_found_response() {
  return "HTTP/1.1 404 Not Found\r\n"
         "Content-Type: text/plain; charset=utf-8\r\n"
         "Content-Length: 9\r\n"
         "Connection: close\r\n"
         "\r\n"
         "not found";
}

bool is_healthz_request(const std::string& request_line) {
  return request_line.rfind("GET /healthz", 0) == 0;
}

bool is_readyz_request(const std::string& request_line) {
  return request_line.rfind("GET /readyz", 0) == 0;
}

void handle_client(int client_fd) {
  char buffer[2048];
  const ssize_t bytes = read(client_fd, buffer, sizeof(buffer) - 1);
  if (bytes <= 0) {
    close(client_fd);
    return;
  }
  buffer[bytes] = '\0';

  const std::string request(buffer);
  const auto line_end = request.find("\r\n");
  const std::string request_line =
      line_end == std::string::npos ? request : request.substr(0, line_end);

  const std::string response = (is_healthz_request(request_line) || is_readyz_request(request_line))
                                   ? make_health_response()
                                   : make_not_found_response();

  if (write(client_fd, response.c_str(), response.size()) < 0) {
    std::cerr << "write failed: " << std::strerror(errno) << '\n';
  }
  close(client_fd);
}

}  // namespace

int main() {
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
  addr.sin_port = htons(kPort);

  if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "bind failed: " << std::strerror(errno) << '\n';
    return 1;
  }

  if (listen(server_fd, 16) < 0) {
    std::cerr << "listen failed: " << std::strerror(errno) << '\n';
    return 1;
  }

  std::cerr << "mancala_server: placeholder HTTP on port " << kPort << " (/healthz)\n";

  while (true) {
    const int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
      std::cerr << "accept failed: " << std::strerror(errno) << '\n';
      continue;
    }
    handle_client(client_fd);
  }

  close(server_fd);
  return 0;
}
