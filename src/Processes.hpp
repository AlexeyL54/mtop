#pragma once

#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Структура, содержащая информацию о процессе
 */
struct ProcessInfo {
  int pid;                   // ID процесса
  std::string name;          // Имя процесса
  char state;                // Состояние процесса (R,S,D,Z,T)
  long vmSize;               // Виртуальный размер памяти (kB)
  long vmRss;                // Резидентный размер памяти (kB)
  long vmPeak;               // Пиковый виртуальный размер памяти (kB)
  long vmHwm;                // Пиковый резидентный размер памяти (kB)
  int threads;               // Количество потоков
  long voluntarySwitches;    // Добровольные переключения контекста
  long nonvoluntarySwitches; // Недобровольные переключения контекста
};

/**
 * @brief Мониторинг процессов системы
 *
 * Фоновый поток периодически собирает информацию о всех процессах
 * из /proc/[pid]/status и предоставляет потокобезопасный доступ к данным.
 */
class Processes {
public:
  /**
   * @brief Конструктор, запускающий фоновый поток сбора данных
   * @param interval_mseconds Интервал обновления данных в миллисекундах (по
   * умолчанию 300)
   */
  explicit Processes(uint32_t interval_mseconds = 300);

  /**
   * @brief Деструктор, останавливающий фоновый поток
   */
  ~Processes();

  // Запрет копирования
  Processes(const Processes &) = delete;
  Processes &operator=(const Processes &) = delete;

  /**
   * @brief Получить копию текущих данных о процессах
   * @return std::vector<ProcessInfo> Вектор с информацией о всех процессах
   */
  std::vector<ProcessInfo> getData() const;

  /**
   * @brief Изменить интервал обновления данных
   * @param interval_msec Новый интервал в миллисекундах
   */
  void setInterval(uint32_t interval_msec);

private:
  mutable std::shared_mutex
      mutex_; ///< Мьютекс для потокобезопасного доступа к data_
  std::atomic<bool> running_{true}; // Флаг работы фонового потока
  std::thread worker_;              // Фоновый поток сбора данных
  std::atomic<uint32_t> interval_;  // Текущий интервал обновления (мс)
  std::vector<ProcessInfo> data_;   // Кэш данных о процессах

  /**
   * @brief Основной цикл фонового потока
   */
  void update();

  /**
   * @brief Получить список всех PID в системе
   * @return std::vector<int> Вектор с PID всех процессов
   */
  std::vector<int> getAllPids();

  /**
   * @brief Прочитать информацию о конкретном процессе из /proc/[pid]/status
   * @param pid ID процесса
   * @param info Ссылка на структуру ProcessInfo для заполнения
   * @return true Если чтение успешно
   * @return false Если процесс не существует или нет доступа
   */
  bool readProcessStatus(int pid, ProcessInfo &info);

  /**
   * @brief Собрать информацию о всех процессах в системе
   */
  void inspectAllPids();
};
