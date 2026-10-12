#pragma once

#include <atomic>
#include <format>
#include <string_view>

namespace db {

enum class LogLevel { kDebug = 0, kInfo = 1, kWarn = 2, kError = 3, kOff = 4 };

// Minimal leveled logger writing single lines to stderr.
//
// Each line is formatted into one buffer and written with a single call, so
// lines from concurrent threads do not interleave mid-line. The initial level
// comes from the MINIDB_LOG_LEVEL environment variable (debug|info|warn|error|
// off), defaulting to warn so tests stay quiet.
class Logger {
 public:
  static LogLevel Level() { return level_.load(std::memory_order_relaxed); }
  static void SetLevel(LogLevel level) { level_.store(level, std::memory_order_relaxed); }
  static bool Enabled(LogLevel level) { return level >= Level(); }

  static void Write(LogLevel level, const char* file, int line, std::string_view msg);

 private:
  static std::atomic<LogLevel> level_;
};

}  // namespace db

// The level check happens before formatting, so disabled log lines cost no
// string building.
#define DB_LOG(level, ...)                                                      \
  do {                                                                          \
    if (::db::Logger::Enabled(level)) {                                         \
      ::db::Logger::Write(level, __FILE__, __LINE__, std::format(__VA_ARGS__)); \
    }                                                                           \
  } while (0)

#define LOG_DEBUG(...) DB_LOG(::db::LogLevel::kDebug, __VA_ARGS__)
#define LOG_INFO(...) DB_LOG(::db::LogLevel::kInfo, __VA_ARGS__)
#define LOG_WARN(...) DB_LOG(::db::LogLevel::kWarn, __VA_ARGS__)
#define LOG_ERROR(...) DB_LOG(::db::LogLevel::kError, __VA_ARGS__)
