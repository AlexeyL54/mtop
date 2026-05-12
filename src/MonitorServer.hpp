#pragma once

#include "Monitor.hpp"
#include <string>

class MonitorServer {
public:
  MonitorServer(uint16_t port, const std::string &webDir);
  void run();
  void stop();

  // Запрещаем копирование
  MonitorServer(const MonitorServer &) = delete;
  MonitorServer &operator=(const MonitorServer &) = delete;

private:
  uint16_t port_;
  std::string webDir_;
  bool running_;

  // Получение экземпляра монитора (синглтон)
  Monitor &getMonitor();

  std::string buildMemoryJson();
  std::string buildCpuJson();
  std::string buildProcessesJson();
  std::string readFile(const std::string &path);
  std::string getMimeType(const std::string &path);
};
