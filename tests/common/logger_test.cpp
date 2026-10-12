#include "db/common/logger.h"

#include <gtest/gtest.h>

namespace db {
namespace {

class LoggerTest : public ::testing::Test {
 protected:
  void SetUp() override { saved_ = Logger::Level(); }
  void TearDown() override { Logger::SetLevel(saved_); }
  LogLevel saved_{};
};

TEST_F(LoggerTest, LevelFiltering) {
  Logger::SetLevel(LogLevel::kWarn);
  EXPECT_FALSE(Logger::Enabled(LogLevel::kInfo));
  EXPECT_TRUE(Logger::Enabled(LogLevel::kWarn));
  EXPECT_TRUE(Logger::Enabled(LogLevel::kError));
}

TEST_F(LoggerTest, DisabledLinesAreNotFormatted) {
  Logger::SetLevel(LogLevel::kOff);
  int evaluated = 0;
  auto side_effect = [&] { return ++evaluated; };
  LOG_ERROR("value {}", side_effect());
  EXPECT_EQ(evaluated, 0);
}

TEST_F(LoggerTest, WritesFormattedLine) {
  Logger::SetLevel(LogLevel::kInfo);
  ::testing::internal::CaptureStderr();
  LOG_INFO("page {} of {}", 3, 10);
  std::string out = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(out.find("[INFO] logger_test.cpp:"), std::string::npos) << out;
  EXPECT_NE(out.find("page 3 of 10\n"), std::string::npos) << out;
}

}  // namespace
}  // namespace db
