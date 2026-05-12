#include "src/MonitorServer.hpp"
#include <iostream>
#include <memory>
#include <signal.h>

std::unique_ptr<MonitorServer> g_server;

/**
 * @brief Обработчик сигнала для graceful shutdown
 */
void signalHandler(int signum) {
  std::cout << "\nReceived signal " << signum << ", shutting down..."
            << std::endl;
  if (g_server) {
    g_server->stop();
  }
  exit(signum);
}

/**
 * @brief Главная функция приложения
 * @param argc Количество аргументов
 * @param argv Аргументы командной строки
 * @return 0 при успешном завершении
 */
int main(int argc, char *argv[]) {
  int port = 8080;

  // Парсим аргументы командной строки
  if (argc > 1) {
    port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
      std::cerr << "Invalid port number. Using default 8080" << std::endl;
      port = 8080;
    }
  }

  // Настройка обработчиков сигналов
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  try {
    g_server = std::make_unique<MonitorServer>(port);
    g_server->run();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
