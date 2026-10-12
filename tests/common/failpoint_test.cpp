#include "db/common/failpoint.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

namespace db {
namespace {

// Stand-in for production code with an injection site.
bool DoWork() {
  if (FAILPOINT("test.do_work")) return false;
  return true;
}

class FailpointTest : public ::testing::Test {
 protected:
  void TearDown() override { FailpointRegistry::Instance().DisarmAll(); }
  FailpointRegistry& reg = FailpointRegistry::Instance();
};

TEST_F(FailpointTest, UnarmedDoesNothing) {
  EXPECT_FALSE(reg.AnyArmed());
  EXPECT_TRUE(DoWork());
  EXPECT_EQ(reg.HitCount("test.do_work"), 0u);
}

TEST_F(FailpointTest, ReturnErrorFiresUntilDisarmed) {
  reg.Arm("test.do_work", FailpointAction::kReturnError);
  EXPECT_TRUE(reg.AnyArmed());
  EXPECT_FALSE(DoWork());
  EXPECT_FALSE(DoWork());
  EXPECT_EQ(reg.HitCount("test.do_work"), 2u);
  reg.Disarm("test.do_work");
  EXPECT_FALSE(reg.AnyArmed());
  EXPECT_TRUE(DoWork());
}

TEST_F(FailpointTest, CountLimitedDisarmsItself) {
  reg.Arm("test.do_work", FailpointAction::kReturnError, /*count=*/2);
  EXPECT_FALSE(DoWork());
  EXPECT_FALSE(DoWork());
  EXPECT_TRUE(DoWork());
  EXPECT_FALSE(reg.AnyArmed());
}

TEST_F(FailpointTest, OtherPointsUnaffected) {
  reg.Arm("test.other", FailpointAction::kReturnError);
  EXPECT_TRUE(DoWork());
}

TEST_F(FailpointTest, PauseBlocksUntilResumed) {
  reg.Arm("test.do_work", FailpointAction::kPause, /*count=*/1);
  std::atomic<bool> done{false};
  std::thread worker([&] {
    EXPECT_TRUE(DoWork());
    done = true;
  });
  // Synchronize on the failpoint itself rather than sleeping.
  reg.WaitUntilPaused("test.do_work");
  EXPECT_FALSE(done);
  reg.Resume("test.do_work");
  worker.join();
  EXPECT_TRUE(done);
}

TEST_F(FailpointTest, DisarmReleasesPausedThread) {
  reg.Arm("test.do_work", FailpointAction::kPause);
  std::thread worker([] { DoWork(); });
  reg.WaitUntilPaused("test.do_work");
  reg.Disarm("test.do_work");
  worker.join();
}

TEST_F(FailpointTest, CrashExitsWithKnownCode) {
  // "threadsafe" re-executes the test binary for the child instead of a bare
  // fork, which is required once other tests have started threads.
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_EXIT(
      {
        FailpointRegistry::Instance().Arm("test.do_work", FailpointAction::kCrash);
        DoWork();
      },
      ::testing::ExitedWithCode(FailpointRegistry::kCrashExitCode), "");
}

}  // namespace
}  // namespace db
