#include "MonitorServer.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

/**
 * @brief Конструктор сервера
 */
MonitorServer::MonitorServer(int port) : port(port), running(true) {
  daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
                            port, nullptr, nullptr, &handleRequest, this,
                            MHD_OPTION_THREAD_POOL_SIZE, 4, MHD_OPTION_END);

  if (!daemon) {
    throw std::runtime_error("Failed to start HTTP server");
  }

  std::cout << "Server started on http://localhost:" << port << std::endl;
  std::cout << "Open in browser: http://localhost:" << port << std::endl;
}

/**
 * @brief Деструктор сервера
 */
MonitorServer::~MonitorServer() {
  if (daemon) {
    MHD_stop_daemon(daemon);
  }
}

/**
 * @brief Запустить сервер
 */
void MonitorServer::run() {
  while (running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

/**
 * @brief Остановить сервер
 */
void MonitorServer::stop() { running = false; }

/**
 * @brief Обработчик HTTP запросов
 */
enum MHD_Result
MonitorServer::handleRequest(void *cls, struct MHD_Connection *connection,
                             const char *url, const char *method,
                             const char *version, const char *upload_data,
                             size_t *upload_data_size, void **con_cls) {
  MonitorServer *server = static_cast<MonitorServer *>(cls);

  // Только GET запросы
  if (std::string(method) != "GET") {
    return MHD_NO;
  }

  // API endpoints
  if (std::string(url) == "/api/processes") {
    std::string json = server->getProcessesJson();
    return server->sendJsonResponse(connection, json);
  } else if (std::string(url) == "/api/memory") {
    std::string json = server->getMemoryJson();
    return server->sendJsonResponse(connection, json);
  } else if (std::string(url) == "/api/cpu") {
    std::string json = server->getCpuJson();
    return server->sendJsonResponse(connection, json);
  } else if (std::string(url) == "/api/all") {
    std::string json = server->getAllJson();
    return server->sendJsonResponse(connection, json);
  } else if (std::string(url) == "/" || std::string(url) == "/index.html") {
    return server->sendHtmlFile(connection, "index.html");
  }

  // 404 Not Found
  return MHD_NO;
}

/**
 * @brief Отправить JSON ответ
 */
enum MHD_Result
MonitorServer::sendJsonResponse(struct MHD_Connection *connection,
                                const std::string &json_data) {
  struct MHD_Response *response = MHD_create_response_from_buffer(
      json_data.length(), (void *)json_data.c_str(), MHD_RESPMEM_MUST_COPY);

  MHD_add_response_header(response, "Content-Type", "application/json");
  MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");

  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return static_cast<MHD_Result>(ret);
}

/**
 * @brief Отправить HTML файл
 */
enum MHD_Result MonitorServer::sendHtmlFile(struct MHD_Connection *connection,
                                            const std::string &filename) {
  std::string filepath = "web/" + filename;
  std::ifstream file(filepath);

  if (!file.is_open()) {
    return MHD_NO;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  struct MHD_Response *response = MHD_create_response_from_buffer(
      content.length(), (void *)content.c_str(), MHD_RESPMEM_MUST_COPY);

  MHD_add_response_header(response, "Content-Type", "text/html");

  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return static_cast<MHD_Result>(ret);
}

/**
 * @brief Создать JSON отчет о процессах
 */
std::string MonitorServer::getProcessesJson() {
  Report report = monitor.getReport();

  std::string json = "{\"processes\":[";

  for (size_t i = 0; i < report.processData.size(); i++) {
    const auto &p = report.processData[i];

    if (i > 0)
      json += ",";

    json += "{";
    json += "\"pid\":" + std::to_string(p.pid) + ",";
    json += "\"name\":\"" + p.name + "\",";
    json += "\"state\":\"" + std::string(1, p.state) + "\",";
    json += "\"vmSize\":" + std::to_string(p.vmSize) + ",";
    json += "\"vmRss\":" + std::to_string(p.vmRss) + ",";
    json += "\"threads\":" + std::to_string(p.threads);
    json += "}";
  }

  json += "],\"total\":" + std::to_string(report.processData.size()) + "}";
  return json;
}

/**
 * @brief Создать JSON отчет о памяти
 */
std::string MonitorServer::getMemoryJson() {
  Report report = monitor.getReport();

  std::string json = "{";
  bool first = true;

  // Выбираем ключевые показатели памяти
  const char *keys[] = {"MemTotal",  "MemFree",  "MemAvailable",
                        "SwapTotal", "SwapFree", "Cached"};

  for (const char *key : keys) {
    if (report.memoryData.find(key) != report.memoryData.end()) {
      if (!first)
        json += ",";
      json += "\"" + std::string(key) +
              "\":" + std::to_string(report.memoryData[key]);
      first = false;
    }
  }

  json += "}";
  return json;
}

/**
 * @brief Создать JSON отчет о CPU
 */
std::string MonitorServer::getCpuJson() {
  Report report = monitor.getReport();

  std::string json = "{\"cores\":[";

  for (size_t i = 0; i < report.cpuData.size(); i++) {
    if (i > 0)
      json += ",";
    json += "{\"id\":" + std::to_string(report.cpuData[i].id) +
            ",\"load\":" + std::to_string(report.cpuData[i].load) + "}";
  }

  json += "],\"total_cores\":" + std::to_string(report.cpuData.size()) + "}";
  return json;
}

/**
 * @brief Создать полный JSON отчет
 */
std::string MonitorServer::getAllJson() {
  // Объединяем все данные в один JSON
  std::string memory = getMemoryJson();
  std::string cpu = getCpuJson();
  std::string processes = getProcessesJson();

  // Удаляем фигурные скобки для вставки
  memory = memory.substr(1, memory.length() - 2);
  cpu = cpu.substr(1, cpu.length() - 2);

  return "{" + memory + "," + cpu + "," + processes.substr(1);
}
