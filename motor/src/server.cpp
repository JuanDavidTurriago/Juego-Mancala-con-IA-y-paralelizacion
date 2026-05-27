#include "alphabeta.h"
#include "mcts.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
  // Placeholder server for local deployment: keep the container alive so the pod stays Running.
  // The real engine will replace this with an HTTP service in a later phase.
  std::cerr << "mancala_server: placeholder process running; HTTP API pending implementation\n";

  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(60));
  }

  return 0;
}

