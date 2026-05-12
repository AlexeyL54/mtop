#include "MonitorServer.hpp"
#include "httplib.h"
#include <fstream>
#include <iostream>
#include <sstream>

/**
 * @brief Конструктор сервера мониторинга
 *
 * @param port Номер порта, на котором будет запущен сервер
 * @param webDir Путь к директории с веб-файлами (HTML, CSS, JS)
 */
MonitorServer::MonitorServer(uint16_t port, const std::string &webDir)
    : port_(port), webDir_(webDir), running_(false) {}

/**
 * @brief Читает содержимое файла
 *
 * @param path Путь к файлу
 * @return std::string Содержимое файла или пустая строка при ошибке
 */
std::string MonitorServer::readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
    return "";
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

/**
 * @brief Определяет MIME тип файла по его расширению
 *
 * @param path Путь к файлу
 * @return std::string MIME тип (text/html, text/css, application/javascript и
 * т.д.)
 */
std::string MonitorServer::getMimeType(const std::string &path) {
  if (path.find(".html") != std::string::npos)
    return "text/html";
  if (path.find(".css") != std::string::npos)
    return "text/css";
  if (path.find(".js") != std::string::npos)
    return "application/javascript";
  if (path.find(".json") != std::string::npos)
    return "application/json";
  return "text/plain";
}

/**
 * @brief Возвращает синглтон монитора системы
 *
 * @return Monitor& Ссылка на единственный экземпляр Monitor
 */
Monitor &MonitorServer::getMonitor() {
  static Monitor monitor;
  return monitor;
}

/**
 * @brief Формирует JSON строку с данными о памяти
 *
 * @return std::string JSON вида {"ключ": значение, ...}
 */
std::string MonitorServer::buildMemoryJson() {
  Report report = getMonitor().getReport();
  std::stringstream json;
  json << "{";
  bool first = true;
  for (const auto &pair : report.memoryData) {
    if (!first)
      json << ",";
    json << "\"" << pair.first << "\":" << pair.second;
    first = false;
  }
  json << "}";
  return json.str();
}

/**
 * @brief Формирует JSON строку с данными о CPU
 *
 * @return std::string JSON с информацией о ядрах и их загрузке
 */
std::string MonitorServer::buildCpuJson() {
  Report report = getMonitor().getReport();
  std::stringstream json;
  json << "{\"total_cores\":" << report.cpuData.size() << ",\"cores\":[";
  for (size_t i = 0; i < report.cpuData.size(); ++i) {
    if (i > 0)
      json << ",";
    json << "{\"id\":" << report.cpuData[i].id
         << ",\"load\":" << report.cpuData[i].load << "}";
  }
  json << "]}";
  return json.str();
}

/**
 * @brief Формирует JSON строку с данными о процессах
 *
 * @return std::string JSON со списком процессов и их параметрами
 */
std::string MonitorServer::buildProcessesJson() {
  Report report = getMonitor().getReport();
  std::stringstream json;
  json << "{\"total\":" << report.processData.size() << ",\"processes\":[";
  for (size_t i = 0; i < report.processData.size(); ++i) {
    if (i > 0)
      json << ",";
    const ProcessInfo &p = report.processData[i];
    json << "{"
         << "\"pid\":" << p.pid << ","
         << "\"name\":\"" << p.name << "\","
         << "\"state\":\"" << p.state << "\","
         << "\"vmSize\":" << p.vmSize << ","
         << "\"vmRss\":" << p.vmRss << ","
         << "\"threads\":" << p.threads << "}";
  }
  json << "]}";
  return json.str();
}

/**
 * @brief Запускает HTTP сервер
 *
 * Метод блокирует выполнение, ожидая входящие соединения.
 * Сервер начинает слушать указанный в конструкторе порт.
 */
void MonitorServer::run() {
  httplib::Server svr;

  // API endpoints
  svr.Get("/api/memory",
          [this](const httplib::Request &req, httplib::Response &res) {
            res.set_content(buildMemoryJson(), "application/json");
          });

  svr.Get("/api/cpu",
          [this](const httplib::Request &req, httplib::Response &res) {
            res.set_content(buildCpuJson(), "application/json");
          });

  svr.Get("/api/processes",
          [this](const httplib::Request &req, httplib::Response &res) {
            res.set_content(buildProcessesJson(), "application/json");
          });

  // Static files
  svr.Get("/", [this](const httplib::Request &req, httplib::Response &res) {
    std::string content = readFile(webDir_ + "/index.html");
    if (content.empty()) {
      res.status = 404;
      res.set_content("Not found", "text/plain");
    } else {
      res.set_content(content, "text/html");
    }
  });

  svr.Get("/(.*)", [this](const httplib::Request &req, httplib::Response &res) {
    std::string path = webDir_ + req.path;
    std::string content = readFile(path);
    if (content.empty()) {
      res.status = 404;
      res.set_content("Not found", "text/plain");
    } else {
      res.set_content(content, getMimeType(path));
    }
  });

  std::cout << "Server listening on http://localhost:" << port_ << std::endl;

  running_ = true;
  svr.listen("localhost", port_);
}

/**
 * @brief Останавливает HTTP сервер
 *
 * Устанавливает флаг остановки сервера.
 * Активные соединения будут завершены.
 */
void MonitorServer::stop() { running_ = false; }
