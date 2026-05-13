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

class Processes {
public:
  explicit Processes(uint32_t interval_mseconds = 200);
  ~Processes();

  std::vector<ProcessInfo> getData() const;
  void setInterval(uint32_t interval_msec);

private:
  mutable std::shared_mutex mutex_;
  std::atomic<bool> running_{true};
  std::thread worker_;
  std::atomic<uint32_t> interval_;
  std::vector<ProcessInfo> data_;

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
