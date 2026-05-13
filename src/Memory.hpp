#pragma once

#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

class Memory {
public:
  explicit Memory(uint32_t interval_msec = 400);
  ~Memory();

  std::unordered_map<std::string, long> getData() const;
  void setInterval(uint32_t interval_msec);

private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, long> data_;
  std::atomic<bool> running_{true};
  std::thread worker_;
  std::atomic<uint32_t> interval_;

  void update();
  std::unordered_map<std::string, long> readMemInfo() const;
};
