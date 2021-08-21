#pragma once

#include <whirl/services/time.hpp>

#include <whirl/engines/matrix/clocks/wall.hpp>
#include <whirl/engines/matrix/clocks/monotonic.hpp>
#include <whirl/engines/matrix/process/scheduler.hpp>

#include <whirl/logger/log.hpp>

#include <await/futures/core/future.hpp>

namespace whirl::matrix {

//////////////////////////////////////////////////////////////////////

// Time service impl

class TimeService : public ITimeService {
 public:
  TimeService(clocks::WallClock& wall_clock,
              clocks::MonotonicClock& monotonic_clock,
              process::Scheduler& scheduler)
      : wall_clock_(wall_clock),
        monotonic_clock_(monotonic_clock),
        scheduler_(scheduler) {
  }

  TimePoint WallTimeNow() override {
    return wall_clock_.Now();
  }

  TimePoint MonotonicNow() override {
    return monotonic_clock_.Now();
  }

  await::futures::Future<void> After(Duration d) override {
    auto after = AfterGlobalTime(d);

    auto [f, p] = await::futures::MakeContract<void>();

    auto cb = [timer_promise = std::move(p)]() mutable {
      std::move(timer_promise).Set();
    };
    Schedule(scheduler_, after, std::move(cb));
    return std::move(f);
  }

 private:
  TimePoint AfterGlobalTime(Duration d) const {
    return GlobalNow() + monotonic_clock_.SleepOrTimeout(d);
  }

 private:
  clocks::WallClock& wall_clock_;
  clocks::MonotonicClock& monotonic_clock_;

  process::Scheduler& scheduler_;

  Logger logger_{"Time"};
};

}  // namespace whirl::matrix
