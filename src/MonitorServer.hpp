#pragma once

// #include "Monitor.hpp"
#include "Cpu.hpp"
#include "Memory.hpp"
#include "Processes.hpp"
#include <string>

/**
 * @brief HTTP сервер для мониторинга системы
 *
 * Предоставляет REST API для получения информации о памяти, CPU и процессах,
 * а также раздаёт статические файлы веб-интерфейса.
 */
class MonitorServer {
public:
  /**
   * @brief Конструктор сервера мониторинга
   *
   * @param port Номер порта, на котором будет запущен сервер
   * @param webDir Путь к директории с веб-файлами (HTML, CSS, JS)
   */
  MonitorServer(uint16_t port, const std::string &webDir);

  /**
   * @brief Запускает HTTP сервер
   *
   * Метод блокирует выполнение, ожидая входящие соединения.
   * Сервер начинает слушать указанный в конструкторе порт.
   */
  void run();

  /**
   * @brief Останавливает HTTP сервер
   *
   * Устанавливает флаг остановки сервера.
   * Активные соединения будут завершены.
   */
  void stop();

  // Запрет копирования
  MonitorServer(const MonitorServer &) = delete;

  // Запрет присваивания копированием
  MonitorServer &operator=(const MonitorServer &) = delete;

private:
  uint16_t port_;      // Порт для HTTP сервера
  std::string webDir_; // Путь к директории с веб-файлами
  bool running_;       // Флаг работы сервера
  Memory memory_monitor_;
  Cpu cpu_monitor_;
  Processes processes_monitor_;

  /**
   * @brief Возвращает синглтон монитора системы
   *
   * @return Monitor& Ссылка на единственный экземпляр Monitor
   */
  // Monitor &getMonitor();

  /**
   * @brief Формирует JSON строку с данными о памяти
   *
   * @return std::string JSON вида {"ключ": значение, ...}
   */
  std::string buildMemoryJson();

  /**
   * @brief Формирует JSON строку с данными о CPU
   *
   * @return std::string JSON с информацией о ядрах и их загрузке
   */
  std::string buildCpuJson();

  /**
   * @brief Формирует JSON строку с данными о процессах
   *
   * @return std::string JSON со списком процессов и их параметрами
   */
  std::string buildProcessesJson();

  /**
   * @brief Читает содержимое файла
   *
   * @param path Путь к файлу
   * @return std::string Содержимое файла или пустая строка при ошибке
   */
  std::string readFile(const std::string &path);

  /**
   * @brief Определяет MIME тип файла по его расширению
   *
   * @param path Путь к файлу
   * @return std::string MIME тип (text/html, text/css, application/javascript и
   * т.д.)
   */
  std::string getMimeType(const std::string &path);
};
