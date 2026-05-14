#pragma once

#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

/**
 * @brief Мониторинг памяти системы
 *
 * Фоновый поток периодически читает /proc/meminfo и предоставляет
 * потокобезопасный доступ к данным о памяти.
 */
class Memory {
public:
  /**
   * @brief Конструктор, запускающий фоновый поток сбора данных
   * @param interval_msec Интервал обновления данных в миллисекундах (по
   * умолчанию 400)
   */
  explicit Memory(uint32_t interval_msec = 4000);

  /**
   * @brief Деструктор, останавливающий фоновый поток
   */
  ~Memory();

  // Запрет копирования
  Memory(const Memory &) = delete;
  Memory &operator=(const Memory &) = delete;

  /**
   * @brief Получить копию текущих данных о памяти
   * @return std::unordered_map<std::string, long> Словарь с параметрами памяти
   *         (ключи: MemTotal, MemFree, MemAvailable и др.)
   */
  std::unordered_map<std::string, long> getData() const;

  /**
   * @brief Изменить интервал обновления данных
   * @param interval_msec Новый интервал в миллисекундах
   */
  void setInterval(uint32_t interval_msec);

private:
  mutable std::shared_mutex
      mutex_; // Мьютекс для потокобезопасного доступа к data_
  std::unordered_map<std::string, long> data_; // Кэш данных о памяти
  std::atomic<bool> running_{true};            // Флаг работы фонового потока
  std::thread worker_;                         // Фоновый поток сбора данных
  std::atomic<uint32_t> interval_; // Текущий интервал обновления (мс)

  /**
   * @brief Основной цикл фонового потока
   */
  void update();

  /**
   * @brief Прочитать данные из /proc/meminfo
   * @return std::unordered_map<std::string, long> Словарь с параметрами памяти
   */
  std::unordered_map<std::string, long> readMemInfo() const;
};
