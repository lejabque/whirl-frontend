#pragma once

#include <whirl/engines/matrix/clocks/drift.hpp>

#include <whirl/engines/matrix/world/global/time.hpp>
#include <whirl/engines/matrix/world/global/time_model.hpp>

namespace whirl::matrix {

class MonotonicClock {
 public:
  MonotonicClock() : drift_(GetTimeModel()->InitClockDrift()) {
    Reset();
  }

  void Reset() {
    init_ = GetTimeModel()->ResetMonotonicClock();
    last_reset_ = GlobalNow();
  }

  TimePoint Now() const {
    return drift_.Elapsed(ElapsedSinceLastReset()) + init_;
  }

  // For timeouts and sleeps
  Duration SleepOrTimeout(Duration d) const {
    return drift_.SleepOrTimeout(d);
  }

 private:
  // Global time
  Duration ElapsedSinceLastReset() const {
    return GlobalNow() - last_reset_;
  }

 private:
  Drift drift_;
  TimePoint last_reset_;
  TimePoint init_;
};


}  // namespace whirl::matrix
