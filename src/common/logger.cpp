#include "db/common/logger.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace db {

namespace {

LogLevel InitialLevel() {
  const char* env = std::getenv("MINIDB_LOG_LEVEL");
  if (env == nullptr) return LogLevel::kWarn;
  std::string_view v(env);
  if (v == "debug") return LogLevel::kDebug;
  if (v == "info") return LogLevel::kInfo;
  if (v == "warn") return LogLevel::kWarn;
  if (v == "error") return LogLevel::kError;
  if (v == "off") return LogLevel::kOff;
  return LogLevel::kWarn;
}

const char* LevelName(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug:
      return "DEBUG";
    case LogLevel::kInfo:
      return "INFO";
    case LogLevel::kWarn:
      return "WARN";
    case LogLevel::kError:
      return "ERROR";
    case LogLevel::kOff:
      return "OFF";
  }
  return "?";
}

std::string_view Basename(const char* path) {
  std::string_view p(path);
  auto slash = p.find_last_of('/');
  return slash == std::string_view::npos ? p : p.substr(slash + 1);
}

}  // namespace

std::atomic<LogLevel> Logger::level_{InitialLevel()};

void Logger::Write(LogLevel level, const char* file, int line, std::string_view msg) {
  std::string out = std::format("[{}] {}:{} {}\n", LevelName(level), Basename(file), line, msg);
  // One fwrite per line: stdio locks the stream per call, so concurrent
  // lines never interleave.
  std::fwrite(out.data(), 1, out.size(), stderr);
}

}  // namespace db
