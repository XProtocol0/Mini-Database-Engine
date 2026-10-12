#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace db {

// Fault injection for tests.
//
// Code under test marks interesting points with FAILPOINT("name"). Tests arm a
// point to make it crash the process, report an error, or pause the calling
// thread until the test resumes it. Unarmed points cost a single relaxed
// atomic load, so they stay compiled into every build.
//
// Usage at an injection site:
//
//   if (FAILPOINT("wal.after_append")) {
//     return <an I/O error>;   // only reached when armed with kReturnError
//   }
//
// The macro evaluates to true only for kReturnError; crash and pause are
// handled inside the call. The site decides what "an error" means, which keeps
// this framework independent of any particular error type.
enum class FailpointAction {
  kOff,
  kReturnError,  // FAILPOINT(...) evaluates to true
  kCrash,        // std::_Exit(kCrashExitCode): no destructors, no flushing
  kPause,        // block until Resume() or Disarm()
};

class FailpointRegistry {
 public:
  // Exit code used by kCrash, so tests can tell an injected crash from others.
  static constexpr int kCrashExitCode = 77;

  static FailpointRegistry& Instance();

  // Arm a failpoint. `count` limits how many hits trigger the action
  // (0 = every hit until disarmed); once used up the point disarms itself.
  void Arm(std::string_view name, FailpointAction action, uint32_t count = 0);
  void Disarm(std::string_view name);
  void DisarmAll();

  // Number of times the point has been reached while armed.
  uint64_t HitCount(std::string_view name) const;

  // For kPause: block until at least one thread is paused at `name`.
  void WaitUntilPaused(std::string_view name);
  // For kPause: release all threads paused at `name` (the point stays armed).
  void Resume(std::string_view name);

  // Called by the FAILPOINT macro. Returns true when the site should fail.
  bool Evaluate(std::string_view name);

  // Fast-path check used by the macro before taking any lock.
  bool AnyArmed() const { return armed_count_.load(std::memory_order_relaxed) > 0; }

 private:
  struct State {
    FailpointAction action = FailpointAction::kOff;
    uint32_t remaining = 0;  // 0 = unlimited
    uint64_t hits = 0;
    uint32_t paused = 0;  // threads currently blocked at this point
    uint64_t resume_epoch = 0;
  };

  State& StateFor(std::string_view name);  // requires mu_
  void SetOff(State& s);                   // requires mu_

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::map<std::string, std::unique_ptr<State>, std::less<>> points_;
  std::atomic<int> armed_count_{0};
};

}  // namespace db

#define FAILPOINT(name)                              \
  (::db::FailpointRegistry::Instance().AnyArmed() && \
   ::db::FailpointRegistry::Instance().Evaluate(name))
