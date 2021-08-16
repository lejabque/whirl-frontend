#pragma once

#include <whirl/time.hpp>

#include <await/futures/core/future.hpp>

#include <memory>

namespace whirl {

struct ITimeService {
  virtual ~ITimeService() = default;

  // Wall clock
  // NB: Constant within simulation step
  virtual TimePoint WallTimeNow() = 0;

  // Monotonic (steady) clock
  // NB: Constant withing simulation step
  virtual TimePoint MonotonicNow() = 0;

  // Timeouts and delays
  virtual await::futures::Future<void> After(Duration d) = 0;
};

}  // namespace whirl
