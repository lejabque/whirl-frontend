#pragma once

#include <whirl/time.hpp>

#include <memory>

namespace whirl {

// TrueTime from Spanner
// Spanner: Google's Globally-Distributed Database"
// https://research.google/pubs/pub39966/

//////////////////////////////////////////////////////////////////////

struct TTInterval {
  TimePoint earliest;
  TimePoint latest;
};

using TTNow = TTInterval;

//////////////////////////////////////////////////////////////////////

struct ITrueTimeService {
  virtual ~ITrueTimeService() = default;

  virtual TTInterval Now() const = 0;

  // True if t has definitely passed
  bool After(TimePoint t) const {
    return Now().earliest > t;
  }

  // True if t has definitely not arrived
  bool Before(TimePoint t) const {
    return Now().latest < t;
  }
};

using ITrueTimeServicePtr = std::shared_ptr<ITrueTimeService>;

}  // namespace whirl
