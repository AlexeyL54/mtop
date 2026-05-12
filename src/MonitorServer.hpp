#pragma once

#include "Monitor.hpp"
#include <atomic>
#include <microhttpd.h>
#include <string>

/**
 * @brief Класс HTTP сервера для мониторинга системы
 *
 * Предоставляет REST API для получения данных о системе:
 * - GET /api/processes - список процессов
 * - GET /api/memory - информация о памяти
 * - GET /api/cpu - загрузка CPU
 * - GET /api/all - полный отчет
 */
class MonitorServer {
public:
  /**
   * @brief Конструктор сервера
   * @param port Порт для HTTP сервера (по умолчанию 8080)
   */
  MonitorServer(int port = 8080);

  /**
   * @brief Деструктор, останавливает сервер
   */
  ~MonitorServer();

  /**
   * @brief Запустить сервер (блокирующий вызов)
   */
  void run();

  /**
   * @brief Остановить сервер
   */
  void stop();

private:
  Monitor monitor;           // Объект для сбора данных
  struct MHD_Daemon *daemon; // Дескриптор HTTP демона
  int port;                  // Порт сервера
  std::atomic<bool> running; // Флаг работы сервера

  /**
   * @brief Обработчик HTTP запросов (статическая функция для libmicrohttpd)
   */
  static enum MHD_Result
  handleRequest(void *cls, struct MHD_Connection *connection, const char *url,
                const char *method, const char *version,
                const char *upload_data, size_t *upload_data_size,
                void **con_cls);

  /**
   * @brief Отправить JSON ответ
   * @param connection HTTP соединение
   * @param json_data JSON строка для отправки
   * @return Результат обработки
   */
  enum MHD_Result sendJsonResponse(struct MHD_Connection *connection,
                                   const std::string &json_data);

  /**
   * @brief Отправить HTML файл из директории web
   * @param connection HTTP соединение
   * @param filename Имя файла
   * @return Результат обработки
   */
  enum MHD_Result sendHtmlFile(struct MHD_Connection *connection,
                               const std::string &filename);

  /**
   * @brief Создать JSON отчет о процессах
   * @return JSON строка со списком процессов
   */
  std::string getProcessesJson();

  /**
   * @brief Создать JSON отчет о памяти
   * @return JSON строка с информацией о памяти
   */
  std::string getMemoryJson();

  /**
   * @brief Создать JSON отчет о CPU
   * @return JSON строка с загрузкой CPU
   */
  std::string getCpuJson();

  /**
   * @brief Создать полный JSON отчет
   * @return JSON строка со всеми данными
   */
  std::string getAllJson();
};
