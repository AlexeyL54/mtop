#include "Monitor.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

/**
 * @brief Конструктор класса Monitor
 *
 * Инициализирует объект монитора. Сбор данных происходит
 * при вызове getReport(), а не в конструкторе.
 */
Monitor::Monitor() {
  // Конструктор пуст, данные собираются при вызове getReport()
}

/**
 * @brief Получить список всех PID в системе
 *
 * Читает содержимое директории /proc и возвращает все поддиректории,
 * имена которых состоят только из цифр (это PID процессов)
 *
 * @return std::vector<int> Вектор с PID всех процессов
 */
std::vector<int> Monitor::getAllPids() {
  std::vector<int> pids;
  DIR *dir = opendir("/proc");
  if (!dir) {
    std::cerr << "Error: Cannot open /proc directory" << std::endl;
    return pids;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    // Проверяем, что имя директории состоит только из цифр (это PID)
    if (entry->d_type == DT_DIR) {
      bool is_number = true;
      for (int i = 0; entry->d_name[i] != '\0'; i++) {
        if (!isdigit(entry->d_name[i])) {
          is_number = false;
          break;
        }
      }
      if (is_number) {
        int pid = std::stoi(entry->d_name);
        pids.push_back(pid);
      }
    }
  }
  closedir(dir);

  // Сортируем PID для удобства
  std::sort(pids.begin(), pids.end());
  return pids;
}

/**
 * @brief Прочитать информацию о процессе из /proc/[pid]/status
 *
 * Открывает файл статуса процесса и парсит его содержимое,
 * заполняя структуру ProcessInfo
 *
 * @param pid ID процесса
 * @param info Ссылка на структуру ProcessInfo для заполнения
 * @return true Если чтение успешно
 * @return false Если процесс не существует или нет доступа
 */
bool Monitor::readProcessStatus(int pid, ProcessInfo &info) {
  std::string status_path = "/proc/" + std::to_string(pid) + "/status";
  std::ifstream status_file(status_path);

  if (!status_file.is_open()) {
    return false;
  }

  info.pid = pid;
  info.vmSize = 0;
  info.vmRss = 0;
  info.vmPeak = 0;
  info.vmHwm = 0;
  info.threads = 0;
  info.voluntarySwitches = 0;
  info.nonvoluntarySwitches = 0;

  std::string line;
  while (std::getline(status_file, line)) {
    if (line.substr(0, 5) == "Name:") {
      info.name = line.substr(6);
      // Убираем пробелы в конце
      info.name.erase(info.name.find_last_not_of(" \t\n\r") + 1);
    } else if (line.substr(0, 6) == "State:") {
      info.state = line.substr(7)[0];
    } else if (line.substr(0, 7) == "VmSize:") {
      info.vmSize = std::stol(line.substr(8));
    } else if (line.substr(0, 6) == "VmRSS:") {
      info.vmRss = std::stol(line.substr(7));
    } else if (line.substr(0, 7) == "VmPeak:") {
      info.vmPeak = std::stol(line.substr(8));
    } else if (line.substr(0, 6) == "VmHWM:") {
      info.vmHwm = std::stol(line.substr(7));
    } else if (line.substr(0, 8) == "Threads:") {
      info.threads = std::stoi(line.substr(9));
    } else if (line.substr(0, 24) == "voluntary_ctxt_switches:") {
      info.voluntarySwitches = std::stol(line.substr(25));
    } else if (line.substr(0, 26) == "nonvoluntary_ctxt_switches:") {
      info.nonvoluntarySwitches = std::stol(line.substr(27));
    }
  }

  status_file.close();
  return true;
}

/**
 * @brief Собрать информацию о памяти из /proc/meminfo
 *
 * Читает файл /proc/meminfo и парсит его в формате "ключ: значение"
 * Результат сохраняется в report.memoryData
 */
void Monitor::inspectMemInfo() {
  std::ifstream meminfo("/proc/meminfo");
  std::string line;
  report.memoryData.clear();

  while (std::getline(meminfo, line)) {
    std::string key;
    long value;
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
      key = line.substr(0, colon_pos);
      // Парсим число, игнорируя "kB"
      value = std::stol(line.substr(colon_pos + 1));
      report.memoryData[key] = value;
    }
  }
}

/**
 * @brief Собрать информацию о всех процессах в системе
 *
 * Получает список всех PID, затем для каждого читает статус
 * и заполняет report.processData валидными процессами
 */
void Monitor::inspectAllPids() {
  pids = getAllPids();
  report.processData.clear();

  for (int pid : pids) {
    ProcessInfo info;
    if (readProcessStatus(pid, info)) {
      report.processData.push_back(info);
    }
  }
}

/**
 * @brief Собрать информацию о загрузке CPU
 *
 * Делает две выборки из /proc/stat с интервалом 1 секунду.
 * Для каждого ядра рассчитывает процент загрузки по формуле:
 * usage = (total - idle) * 100 / total
 * Результат сохраняется в report.cpuData
 */
void Monitor::inspectCpuLoad() {
  std::ifstream stat_file("/proc/stat");
  if (!stat_file.is_open()) {
    std::cerr << "Error: Cannot open /proc/stat" << std::endl;
    return;
  }

  std::string line;
  first_samples.clear();

  // Первая выборка
  while (std::getline(stat_file, line)) {
    if (line.substr(0, 3) != "cpu" || line[3] == ' ')
      continue;

    std::istringstream iss(line);
    std::string cpu_name;
    iss >> cpu_name;

    std::vector<unsigned long long> values;
    unsigned long long val;
    while (iss >> val)
      values.push_back(val);

    if (values.size() >= 4) {
      first_samples.push_back(values);
    }
  }
  stat_file.close();

  // Ожидание 1 секунду
  sleep(1);

  // Вторая выборка
  stat_file.open("/proc/stat");
  second_samples.clear();

  while (std::getline(stat_file, line)) {
    if (line.substr(0, 3) != "cpu" || line[3] == ' ')
      continue;

    std::istringstream iss(line);
    std::string cpu_name;
    iss >> cpu_name;

    std::vector<unsigned long long> values;
    unsigned long long val;
    while (iss >> val)
      values.push_back(val);

    if (values.size() >= 4) {
      second_samples.push_back(values);
    }
  }
  stat_file.close();

  // Заполняем report.cpuData
  report.cpuData.clear();
  for (size_t i = 0; i < first_samples.size() && i < second_samples.size();
       ++i) {
    unsigned long long prev_idle = first_samples[i][3] + first_samples[i][4];
    unsigned long long prev_total = 0;
    for (auto v : first_samples[i])
      prev_total += v;

    unsigned long long curr_idle = second_samples[i][3] + second_samples[i][4];
    unsigned long long curr_total = 0;
    for (auto v : second_samples[i])
      curr_total += v;

    unsigned long long diff_idle = curr_idle - prev_idle;
    unsigned long long diff_total = curr_total - prev_total;

    double usage = (diff_total - diff_idle) * 100.0 / diff_total;

    Core core;
    core.id = static_cast<uint32_t>(i);
    core.load = static_cast<float>(usage);
    report.cpuData.push_back(core);
  }
}

/**
 * @brief Получить полный отчёт о состоянии системы
 *
 * Собирает актуальные данные о памяти, CPU и процессах
 * и возвращает структуру Report
 *
 * @return Report Структура с данными о памяти, CPU и процессах
 */
Report Monitor::getReport() {
  inspectMemInfo();
  inspectCpuLoad();
  inspectAllPids();
  return report;
}

/**
 * @brief Вывести отчёт в консоль (для отладки)
 *
 * Форматированный вывод всей собранной информации:
 * - Данные о памяти
 * - Загрузка CPU по ядрам
 * - Таблица процессов
 */
void Monitor::printReport() {
  // Вывод информации о памяти
  std::cout << "Total Memory: " << report.memoryData["MemTotal"] << " kB\n";
  std::cout << "Free Memory: " << report.memoryData["MemFree"] << " kB\n";
  std::cout << "Available Memory: " << report.memoryData["MemAvailable"]
            << " kB\n";

  // Вывод информации о CPU
  std::cout << "\nCPU Load per Core:" << std::endl;
  for (const auto &core : report.cpuData) {
    std::cout << "Core " << core.id << ": " << core.load << "%" << std::endl;
  }

  // Вывод информации о процессах
  std::cout << "\n=== System Processes Information ===" << std::endl;
  std::cout << "Total processes: " << report.processData.size() << "\n"
            << std::endl;

  // Заголовки таблицы
  std::cout << std::left << std::setw(8) << "PID" << std::setw(20) << "Name"
            << std::setw(8) << "State" << std::setw(12) << "VmSize(KB)"
            << std::setw(12) << "VmRSS(KB)" << std::setw(10) << "Threads"
            << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  for (const auto &info : report.processData) {
    std::cout << std::left << std::setw(8) << info.pid << std::setw(20)
              << (info.name.length() > 19 ? info.name.substr(0, 19) : info.name)
              << std::setw(8) << info.state << std::setw(12) << info.vmSize
              << std::setw(12) << info.vmRss << std::setw(10) << info.threads
              << std::endl;
  }
  std::cout << "=====================================" << std::endl;
}
