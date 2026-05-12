#include "Monitor.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

Monitor::Monitor() {
  // Инициализация
}

std::vector<int> Monitor::getAllPids() {
  std::vector<int> pids;
  DIR *dir = opendir("/proc");
  if (!dir) {
    std::cerr << "Error: Cannot open /proc directory" << std::endl;
    return pids;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_type == DT_DIR) {
      bool is_number = true;
      for (int i = 0; entry->d_name[i] != '\0'; i++) {
        if (!std::isdigit(static_cast<unsigned char>(entry->d_name[i]))) {
          is_number = false;
          break;
        }
      }
      if (is_number) {
        try {
          int pid = std::stoi(entry->d_name);
          pids.push_back(pid);
        } catch (const std::exception &e) {
          // Игнорируем ошибки конвертации
        }
      }
    }
  }
  closedir(dir);

  std::sort(pids.begin(), pids.end());
  return pids;
}

bool Monitor::readProcessStatus(int pid, ProcessInfo &info) {
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
      // Убираем пробелы в конце
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

  status_file.close();

  if (info.name.empty()) {
    info.name = "unknown";
  }

  return true;
}

void Monitor::inspectMemInfo() {
  std::ifstream meminfo("/proc/meminfo");
  if (!meminfo.is_open()) {
    std::cerr << "Warning: Cannot open /proc/meminfo" << std::endl;
    return;
  }

  std::string line;
  report_.memoryData.clear();

  while (std::getline(meminfo, line)) {
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos && colon_pos > 0) {
      std::string key = line.substr(0, colon_pos);
      try {
        // Парсим число, игнорируя "kB" и пробелы
        size_t start = colon_pos + 1;
        while (start < line.length() && std::isspace(line[start]))
          start++;

        std::string num_str;
        while (start < line.length() && std::isdigit(line[start])) {
          num_str += line[start];
          start++;
        }

        if (!num_str.empty()) {
          long value = std::stol(num_str);
          report_.memoryData[key] = value;
        }
      } catch (const std::exception &e) {
        // Игнорируем ошибки парсинга
      }
    }
  }
}

void Monitor::inspectAllPids() {
  pids_ = getAllPids();
  report_.processData.clear();
  report_.processData.reserve(pids_.size());

  for (int pid : pids_) {
    ProcessInfo info = {};
    if (readProcessStatus(pid, info)) {
      report_.processData.push_back(info);
    }
  }
}

void Monitor::inspectCpuLoad() {
  std::ifstream stat_file("/proc/stat");
  if (!stat_file.is_open()) {
    std::cerr << "Error: Cannot open /proc/stat" << std::endl;
    return;
  }

  std::string line;
  std::vector<std::vector<unsigned long long>> first_samples;

  // Первая выборка
  while (std::getline(stat_file, line)) {
    if (line.substr(0, 3) != "cpu" || (line.length() > 3 && line[3] == ' ')) {
      continue;
    }

    std::istringstream iss(line);
    std::string cpu_name;
    iss >> cpu_name;

    std::vector<unsigned long long> values;
    unsigned long long val;
    while (iss >> val) {
      values.push_back(val);
    }

    if (values.size() >= 4) {
      first_samples.push_back(values);
    }
  }
  stat_file.close();

  if (first_samples.empty()) {
    std::cerr << "Warning: No CPU data found" << std::endl;
    return;
  }

  // Ожидание 1 секунду
  sleep(1);

  // Вторая выборка
  stat_file.open("/proc/stat");
  std::vector<std::vector<unsigned long long>> second_samples;

  while (std::getline(stat_file, line)) {
    if (line.substr(0, 3) != "cpu" || (line.length() > 3 && line[3] == ' ')) {
      continue;
    }

    std::istringstream iss(line);
    std::string cpu_name;
    iss >> cpu_name;

    std::vector<unsigned long long> values;
    unsigned long long val;
    while (iss >> val) {
      values.push_back(val);
    }

    if (values.size() >= 4) {
      second_samples.push_back(values);
    }
  }
  stat_file.close();

  // Заполняем report_.cpuData
  report_.cpuData.clear();
  size_t num_cores = std::min(first_samples.size(), second_samples.size());

  for (size_t i = 0; i < num_cores; ++i) {
    unsigned long long prev_idle =
        first_samples[i][3] + first_samples[i][4]; // idle + iowait
    unsigned long long prev_total = 0;
    for (auto v : first_samples[i])
      prev_total += v;

    unsigned long long curr_idle = second_samples[i][3] + second_samples[i][4];
    unsigned long long curr_total = 0;
    for (auto v : second_samples[i])
      curr_total += v;

    unsigned long long diff_idle = curr_idle - prev_idle;
    unsigned long long diff_total = curr_total - prev_total;

    double usage = 0.0;
    if (diff_total > 0) {
      usage = (diff_total - diff_idle) * 100.0 / diff_total;
    }

    Core core;
    core.id = static_cast<uint32_t>(i);
    core.load = static_cast<float>(usage);
    report_.cpuData.push_back(core);
  }
}

Report Monitor::getReport() {
  std::lock_guard<std::mutex> lock(report_mutex_);
  try {
    inspectMemInfo();
    inspectCpuLoad();
    inspectAllPids();
  } catch (const std::exception &e) {
    std::cerr << "Error collecting system data: " << e.what() << std::endl;
  }
  return report_;
}
