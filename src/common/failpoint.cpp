#include "db/common/failpoint.h"

#include <cstdlib>

namespace db {

FailpointRegistry& FailpointRegistry::Instance() {
  static FailpointRegistry registry;
  return registry;
}

FailpointRegistry::State& FailpointRegistry::StateFor(std::string_view name) {
  auto it = points_.find(name);
  if (it == points_.end()) {
    it = points_.emplace(std::string(name), std::make_unique<State>()).first;
  }
  return *it->second;
}

void FailpointRegistry::SetOff(State& s) {
  if (s.action != FailpointAction::kOff) {
    armed_count_.fetch_sub(1, std::memory_order_relaxed);
  }
  s.action = FailpointAction::kOff;
  s.remaining = 0;
}

void FailpointRegistry::Arm(std::string_view name, FailpointAction action, uint32_t count) {
  if (action == FailpointAction::kOff) {
    Disarm(name);
    return;
  }
  std::lock_guard lock(mu_);
  State& s = StateFor(name);
  if (s.action == FailpointAction::kOff) {
    armed_count_.fetch_add(1, std::memory_order_relaxed);
  }
  s.action = action;
  s.remaining = count;
  s.hits = 0;
}

void FailpointRegistry::Disarm(std::string_view name) {
  {
    std::lock_guard lock(mu_);
    auto it = points_.find(name);
    if (it == points_.end()) return;
    SetOff(*it->second);
    // Disarming also releases any thread paused at this point.
    ++it->second->resume_epoch;
  }
  cv_.notify_all();
}

void FailpointRegistry::DisarmAll() {
  {
    std::lock_guard lock(mu_);
    for (auto& [_, s] : points_) {
      SetOff(*s);
      ++s->resume_epoch;
    }
  }
  cv_.notify_all();
}

uint64_t FailpointRegistry::HitCount(std::string_view name) const {
  std::lock_guard lock(mu_);
  auto it = points_.find(name);
  return it == points_.end() ? 0 : it->second->hits;
}

void FailpointRegistry::WaitUntilPaused(std::string_view name) {
  std::unique_lock lock(mu_);
  State& s = StateFor(name);
  cv_.wait(lock, [&] { return s.paused > 0; });
}

void FailpointRegistry::Resume(std::string_view name) {
  {
    std::lock_guard lock(mu_);
    ++StateFor(name).resume_epoch;
  }
  cv_.notify_all();
}

bool FailpointRegistry::Evaluate(std::string_view name) {
  std::unique_lock lock(mu_);
  auto it = points_.find(name);
  if (it == points_.end() || it->second->action == FailpointAction::kOff) return false;
  State& s = *it->second;

  ++s.hits;
  const FailpointAction action = s.action;
  // A count-limited point disarms itself after its last triggering hit. The
  // action for *this* hit was captured above, so it still runs.
  if (s.remaining > 0 && --s.remaining == 0) {
    SetOff(s);
  }

  switch (action) {
    case FailpointAction::kReturnError:
      return true;
    case FailpointAction::kCrash:
      // _Exit skips destructors and stdio flushing, which is the closest
      // in-process approximation of a power cut or SIGKILL.
      std::_Exit(kCrashExitCode);
    case FailpointAction::kPause: {
      // Wait for the epoch to change rather than for the action to become
      // kOff: a count-limited pause has already turned itself off above, but
      // this thread must still stay paused until the test resumes it.
      const uint64_t epoch = s.resume_epoch;
      ++s.paused;
      cv_.notify_all();  // wake WaitUntilPaused()
      cv_.wait(lock, [&] { return s.resume_epoch != epoch; });
      --s.paused;
      return false;
    }
    case FailpointAction::kOff:
      break;
  }
  return false;
}

}  // namespace db
