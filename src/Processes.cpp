// Processes.cpp
#include "Processes.hpp"
#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

/**
 * @brief Конструктор, запускающий фоновый поток сбора данных
 * @param interval_mseconds Интервал обновления данных в миллисекундах (по
 * умолчанию 200)
 */
Processes::Processes(uint32_t interval_msec) : interval_(interval_msec) {
  worker_ = std::thread([this]() { update(); });
}

/**
 * @brief Деструктор, останавливающий фоновый поток
 */
Processes::~Processes() {
  running_ = false;
  if (worker_.joinable()) {
    worker_.join();
  }
}

/**
 * @brief Получить копию текущих данных о процессах
 * @return std::vector<ProcessInfo> Вектор с информацией о всех процессах
 */
std::vector<ProcessInfo> Processes::getData() const {
  std::shared_lock lock(mutex_);
  return data_; // Копируем данные
}

/**
 * @brief Изменить интервал обновления данных
 * @param interval_msec Новый интервал в миллисекундах
 */
void Processes::setInterval(uint32_t interval_msec) {
  interval_ = interval_msec;
}

/**
 * @brief Основной цикл фонового потока
 */
void Processes::update() {
  while (running_) {
    try {
      std::vector<int> new_pids = getAllPids();

      std::vector<ProcessInfo> new_data;
      new_data.reserve(new_pids.size());

      for (int pid : new_pids) {
        ProcessInfo info;
        if (readProcessStatus(pid, info)) {
          new_data.push_back(std::move(info));
        }
      }

      {
        std::unique_lock lock(mutex_);
        data_ = std::move(new_data);
      }

    } catch (const std::exception &e) {
      std::cerr << "Processes: Error: " << e.what() << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(interval_.load()));
  }
}

/**
 * @brief Получить список всех PID в системе
 * @return std::vector<int> Вектор с PID всех процессов
 */
std::vector<int> Processes::getAllPids() {
  std::vector<int> pids;
  DIR *dir = opendir("/proc");
  if (!dir) {
    std::cerr << "Error: Cannot open /proc directory" << std::endl;
    return pids;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_type == DT_DIR) {
      // Проверяем, что имя состоит только из цифр
      bool is_number = true;
      for (int i = 0; entry->d_name[i] != '\0'; i++) {
        if (!std::isdigit(static_cast<unsigned char>(entry->d_name[i]))) {
          is_number = false;
          break;
        }
      }

      if (is_number) {
        try {
          pids.push_back(std::stoi(entry->d_name));
        } catch (const std::exception &) {
          // Игнорируем
        }
      }
    }
  }
  closedir(dir);

  std::sort(pids.begin(), pids.end());
  return pids;
}

/**
 * @brief Прочитать информацию о конкретном процессе из /proc/[pid]/status
 * @param pid ID процесса
 * @param info Ссылка на структуру ProcessInfo для заполнения
 * @return true Если чтение успешно
 * @return false Если процесс не существует или нет доступа
 */
bool Processes::readProcessStatus(int pid, ProcessInfo &info) {
  std::string status_path = "/proc/" + std::to_string(pid) + "/status";
  std::ifstream status_file(status_path);

  if (!status_file.is_open()) {
    return false;
  }

  info = ProcessInfo{};
  info.pid = pid;

  std::string line;
  while (std::getline(status_file, line)) {
    if (line.length() < 5)
      continue;

    if (line.substr(0, 5) == "Name:") {
      info.name = line.substr(6);
      auto end = info.name.find_last_not_of(" \t\n\r");
      if (end != std::string::npos) {
        info.name.erase(end + 1);
      }
    } else if (line.substr(0, 6) == "State:") {
      if (line.length() > 7) {
        info.state = line.substr(7)[0];
      }
    } else if (line.substr(0, 7) == "VmSize:") {
      try {
        info.vmSize = std::stol(line.substr(8));
      } catch (...) {
      }
    } else if (line.substr(0, 6) == "VmRSS:") {
      try {
        info.vmRss = std::stol(line.substr(7));
      } catch (...) {
      }
    } else if (line.substr(0, 7) == "VmPeak:") {
      try {
        info.vmPeak = std::stol(line.substr(8));
      } catch (...) {
      }
    } else if (line.substr(0, 6) == "VmHWM:") {
      try {
        info.vmHwm = std::stol(line.substr(7));
      } catch (...) {
      }
    } else if (line.substr(0, 8) == "Threads:") {
      try {
        info.threads = std::stoi(line.substr(9));
      } catch (...) {
      }
    } else if (line.substr(0, 24) == "voluntary_ctxt_switches:") {
      try {
        info.voluntarySwitches = std::stol(line.substr(25));
      } catch (...) {
      }
    } else if (line.substr(0, 26) == "nonvoluntary_ctxt_switches:") {
      try {
        info.nonvoluntarySwitches = std::stol(line.substr(27));
      } catch (...) {
      }
    }
  }

  if (info.name.empty()) {
    info.name = "unknown";
  }

  return true;
}
