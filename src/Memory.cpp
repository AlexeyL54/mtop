#include "Memory.hpp"
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>

/**
 * @brief Конструктор, запускающий фоновый поток сбора данных
 * @param interval_msec Интервал обновления данных в миллисекундах (по умолчанию
 * 400)
 */
Memory::Memory(uint32_t interval_msec) : interval_(interval_msec) {
  worker_ = std::thread([this]() { update(); });
};

/**
 * @brief Деструктор, останавливающий фоновый поток
 */
Memory::~Memory() {
  running_ = false;
  if (worker_.joinable()) {
    worker_.join();
  }
}

/**
 * @brief Получить копию текущих данных о памяти
 * @return std::unordered_map<std::string, long> Словарь с параметрами памяти
 *         (ключи: MemTotal, MemFree, MemAvailable и др.)
 */
std::unordered_map<std::string, long> Memory::getData() const {
  std::shared_lock lock(mutex_);
  return data_;
}

/**
 * @brief Изменить интервал обновления данных
 * @param interval_msec Новый интервал в миллисекундах
 */
void Memory::setInterval(uint32_t interval_msec) { interval_ = interval_msec; }

/**
 * @brief Основной цикл фонового потока
 */
void Memory::update() {
  while (running_) {

    try {
      std::unordered_map<std::string, long> new_data = readMemInfo();

      {
        std::unique_lock lock(mutex_);
        data_ = std::move(new_data);
      }

    } catch (const std::exception &e) {
      std::cerr << "Memory: Error reading /proc/meminfo: " << e.what()
                << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(interval_.load()));
  }
}

/**
 * @brief Прочитать данные из /proc/meminfo
 * @return std::unordered_map<std::string, long> Словарь с параметрами памяти
 */
std::unordered_map<std::string, long> Memory::readMemInfo() const {
  std::ifstream meminfo("/proc/meminfo");
  if (!meminfo.is_open()) {
    throw std::runtime_error("Cannot open /proc/meminfo");
  }

  std::unordered_map<std::string, long> data;
  std::string line;

  while (std::getline(meminfo, line)) {
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos || colon_pos == 0) {
      continue;
    }

    std::string key = line.substr(0, colon_pos);

    // Парсим число после двоеточия
    size_t start = colon_pos + 1;
    while (start < line.length() && std::isspace(line[start])) {
      ++start;
    }

    std::string num_str;
    while (start < line.length() && std::isdigit(line[start])) {
      num_str += line[start];
      ++start;
    }

    if (!num_str.empty()) {
      try {
        long value = std::stol(num_str);
        data[key] = value;
      } catch (const std::exception &) {
        // Игнорируем ошибки парсинга отдельных строк
      }
    }
  }

  return data;
}
