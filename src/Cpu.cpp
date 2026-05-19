#include "Cpu.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

/**
 * @brief Конструктор запускает фоновый поток обновления
 * @param interval_msec Интервал между обновлениями в миллисекундах (по
 * умолчанию 2000)
 * @note Реальная задержка между замерами = interval_msec,
 *       но внутри делается sleep(1) для двух сэмплов
 */
Cpu::Cpu(uint32_t interval_msec) : interval_(interval_msec) {
  worker_ = std::thread([this]() { update(); });
}

/**
 * @brief Деструктор, останавливающий фоновый поток
 */
Cpu::~Cpu() {
  running_ = false;
  if (worker_.joinable()) {
    worker_.join();
  }
}

/**
 * @brief Получить текущие данные о загрузке CPU (потокобезопасно)
 * @return std::vector<Core> Копия актуальных данных
 */
std::vector<Core> Cpu::getData() const {
  std::shared_lock lock(mutex_);
  return data_;
}

/**
 * @brief Изменить интервал обновления на лету
 * @param interval_msec Новый интервал в миллисекундах
 */
void Cpu::setInterval(uint32_t interval_msec) { interval_ = interval_msec; }

/**
 * @brief Основной цикл фонового потока
 */
void Cpu::update() {
  while (running_) {
    try {
      // Первый замер
      std::vector<std::vector<unsigned long long>> first_sample =
          getCpuSample();

      if (first_sample.empty()) {
        std::cerr << "Cpu: No CPU data on first sample" << std::endl;
        std::this_thread::sleep_for(
            std::chrono::milliseconds(interval_.load()));
        continue;
      }

      // Ждём 1 секунду между замерами (фиксированно, не зависит от interval_)
      std::this_thread::sleep_for(std::chrono::seconds(1));

      if (!running_)
        break;

      // Второй замер
      std::vector<std::vector<unsigned long long>> second_sample =
          getCpuSample();

      if (second_sample.empty()) {
        std::cerr << "Cpu: No CPU data on second sample" << std::endl;
        std::this_thread::sleep_for(
            std::chrono::milliseconds(interval_.load()));
        continue;
      }

      // Вычисляем загрузку
      std::vector<Core> fresh_data =
          calculateCpuLoad(first_sample, second_sample);

      {
        std::unique_lock lock(mutex_);
        data_ = std::move(fresh_data);
      }

    } catch (const std::exception &e) {
      std::cerr << "Cpu: Error: " << e.what() << std::endl;
    }

    uint32_t remaining_ms = interval_.load();
    if (remaining_ms > 1000) {
      remaining_ms -= 1000;
    }

    if (remaining_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(remaining_ms));
    }
  }
}

/**
 * @brief Получить сэмпл загрузки CPU из /proc/stat
 * @return std::vector<std::vector<unsigned long long>> Вектор с данными по
 * каждому ядру
 */
std::vector<std::vector<unsigned long long>> Cpu::getCpuSample() const {
  std::ifstream stat_file("/proc/stat");
  if (!stat_file.is_open()) {
    throw std::runtime_error("Cannot open /proc/stat");
  }

  std::vector<std::vector<unsigned long long>> samples;
  std::string line;

  while (std::getline(stat_file, line)) {
    // Ищем строки "cpu0", "cpu1", ... но не "cpu " (общая статистика)
    if (line.substr(0, 3) != "cpu")
      continue;
    if (line.length() > 3 && line[3] == ' ')
      continue;

    std::istringstream iss(line);
    std::string cpu_name;
    iss >> cpu_name;

    std::vector<unsigned long long> values;
    unsigned long long val;
    while (iss >> val) {
      values.push_back(val);
    }

    // Нам нужно минимум 4 значения: user, nice, system, idle
    if (values.size() >= 4) {
      samples.push_back(values);
    }
  }

  return samples;
}

/**
 * @brief Рассчитать загрузку CPU на основе двух сэмплов
 * @param prev_sample Предыдущий сэмпл
 * @param curr_sample Текущий сэмпл
 * @return std::vector<Core> Вектор с загрузкой каждого ядра
 */
std::vector<Core> Cpu::calculateCpuLoad(
    const std::vector<std::vector<unsigned long long>> &prev_sample,
    const std::vector<std::vector<unsigned long long>> &curr_sample) const {

  std::vector<Core> cpu_load;
  size_t num_cores = std::min(prev_sample.size(), curr_sample.size());

  for (size_t i = 0; i < num_cores; ++i) {
    // Idle время = idle + iowait
    unsigned long long prev_idle = prev_sample[i][3] + prev_sample[i][4];
    unsigned long long curr_idle = curr_sample[i][3] + curr_sample[i][4];

    // Общее время = сумма всех значений
    unsigned long long prev_total = 0;
    for (unsigned long long v : prev_sample[i]) {
      prev_total += v;
    }

    unsigned long long curr_total = 0;
    for (unsigned long long v : curr_sample[i]) {
      curr_total += v;
    }

    unsigned long long diff_idle = curr_idle - prev_idle;
    unsigned long long diff_total = curr_total - prev_total;

    double usage = 0.0;
    if (diff_total > 0) {
      usage = (diff_total - diff_idle) * 100.0 / diff_total;
    }

    Core core;
    core.id = static_cast<uint32_t>(i);
    core.load = static_cast<float>(usage);
    cpu_load.push_back(core);
  }

  return cpu_load;
}
