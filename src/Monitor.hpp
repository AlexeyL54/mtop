#pragma once

#include <cstdint>
#include <dirent.h>
#include <mutex>
#include <string>
#include <unistd.h>
#include <unordered_map>
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
 * @brief Структура, содержащая информацию о ядре CPU
 */
struct Core {
  uint32_t id; // Номер ядра
  float load;  // Загрузка ядра в процентах (0.0-100.0)
};

/**
 * @brief Структура, содержащая полный отчёт о состоянии системы
 */
struct Report {
  std::unordered_map<std::string, long> memoryData; // Данные из /proc/meminfo
  std::vector<ProcessInfo> processData;             // Список процессов
  std::vector<Core> cpuData; // Данные о загрузке ядер CPU
};

/**
 * @brief Класс для мониторинга системных ресурсов Linux
 */
class Monitor {
public:
  Monitor();
  /**
   * @brief Получить полный отчёт о состоянии системы
   * @return Report Структура с данными о памяти, CPU и процессах
   */
  Report getReport();

  /**
   * @brief Вывести отчёт в консоль (для отладки)
   */
  void printReport();

  // Запрещаем копирование
  Monitor(const Monitor &) = delete;
  Monitor &operator=(const Monitor &) = delete;

private:
  std::mutex report_mutex_; // Мьютекс для потокобезопасности
  std::unordered_map<std::string, long> memory_data_; // Данных о памяти
  std::vector<int> pids_;                             // Список PID процессов
  Report report_;                                     // Актуальный отчёт

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

  /**
   * @brief Собрать информацию о памяти из /proc/meminfo
   */
  void inspectMemInfo();

  /**
   * @brief Собрать информацию о загрузке CPU
   * @details Метод делает два замера с интервалом в 1 секунду
   *          и вычисляет загрузку каждого ядра
   */
  void inspectCpuLoad();

  /**
   * @brief Получить сэмпл загрузки CPU из /proc/stat
   * @return std::vector<std::vector<unsigned long long>> Вектор с данными по
   * каждому ядру Каждый внутренний вектор содержит значения: user, nice,
   * system, idle, iowait, irq, softirq, steal, guest, guest_nice
   */
  std::vector<std::vector<unsigned long long>> getCpuSample();

  /**
   * @brief Рассчитать загрузку CPU на основе двух сэмплов
   * @param prev_sample Предыдущий сэмпл
   * @param curr_sample Текущий сэмпл
   * @return std::vector<Core> Вектор с загрузкой каждого ядра
   */
  std::vector<Core> calculateCpuLoad(
      const std::vector<std::vector<unsigned long long>> &prev_sample,
      const std::vector<std::vector<unsigned long long>> &curr_sample);
};
