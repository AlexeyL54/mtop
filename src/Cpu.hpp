#pragma once

#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <thread>
#include <vector>

/**
 * @brief Структура, содержащая информацию о ядре CPU
 */
struct Core {
  uint32_t id; // Номер ядра
  float load;  // Загрузка ядра в процентах (0.0-100.0)
};

/**
 * @brief Монитор загрузки CPU
 *
 * Фоновый поток делает два замера с интервалом 1 секунду
 * и вычисляет загрузку каждого ядра.
 */
class Cpu {
public:
  /**
   * @brief Конструктор запускает фоновый поток обновления
   * @param interval_msec Интервал между обновлениями в миллисекундах (по
   * умолчанию 2000)
   * @note Реальная задержка между замерами = interval_msec,
   *       но внутри делается sleep(1) для двух сэмплов
   */
  explicit Cpu(uint32_t interval_msec = 2000);

  /**
   * @brief Деструктор, останавливающий фоновый поток
   */
  ~Cpu();

  // Запрет копирования
  Cpu(const Cpu &) = delete;
  Cpu &operator=(const Cpu &) = delete;

  /**
   * @brief Получить текущие данные о загрузке CPU (потокобезопасно)
   * @return std::vector<Core> Копия актуальных данных
   */
  std::vector<Core> getData() const;

  /**
   * @brief Изменить интервал обновления на лету
   * @param interval_msec Новый интервал в миллисекундах
   */
  void setInterval(uint32_t interval_msec);

private:
  mutable std::shared_mutex mutex_;
  std::vector<Core> data_;
  std::atomic<bool> running_{true};
  std::thread worker_;
  std::atomic<uint32_t> interval_;

  /**
   * @brief Основной цикл фонового потока
   */
  void update();

  /**
   * @brief Получить сэмпл загрузки CPU из /proc/stat
   * @return std::vector<std::vector<unsigned long long>> Вектор с данными по
   * каждому ядру
   */
  std::vector<std::vector<unsigned long long>> getCpuSample() const;

  /**
   * @brief Рассчитать загрузку CPU на основе двух сэмплов
   * @param prev_sample Предыдущий сэмпл
   * @param curr_sample Текущий сэмпл
   * @return std::vector<Core> Вектор с загрузкой каждого ядра
   */
  std::vector<Core> calculateCpuLoad(
      const std::vector<std::vector<unsigned long long>> &prev_sample,
      const std::vector<std::vector<unsigned long long>> &curr_sample) const;
};
