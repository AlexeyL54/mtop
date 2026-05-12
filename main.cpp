#include "src/MonitorServer.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
  uint16_t port = 8080;
  std::string webDir =
      "./web"; // Убедитесь, что папка web существует и содержит index.html

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-p" && i + 1 < argc) {
      port = std::stoi(argv[++i]);
    } else if (arg == "-d" && i + 1 < argc) {
      webDir = argv[++i];
    } else if (arg == "-h") {
      std::cout << "Usage: " << argv[0] << " [-p port] [-d web-dir]"
                << std::endl;
      return 0;
    }
  }

  std::cout << "Starting mtop server on http://localhost:" << port << std::endl;
  std::cout << "Web interface: http://localhost:" << port << std::endl;
  std::cout << "Press Ctrl+C to stop" << std::endl;

  try {
    MonitorServer server(port, webDir);
    server.run(); // run() не должен возвращать управление до остановки
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
