#pragma once

#include <whirl/rpc/method.hpp>
#include <whirl/rpc/bytes_value.hpp>
#include <whirl/rpc/trace.hpp>

#include <await/futures/core/future.hpp>
#include <await/util/stop_token.hpp>

#include <memory>

namespace whirl::rpc {

struct CallOptions {
  TraceId trace_id;
  await::StopToken stop_advice;
  size_t attempts_limit;
};

// Communication line between client and remote service
// Untyped

struct IChannel {
  virtual ~IChannel() = default;

  // Unary RPC call
  virtual await::futures::Future<BytesValue> Call(const Method& method,
                                                  const BytesValue& input,
                                                  CallOptions options) = 0;

  virtual void Close() = 0;

  virtual const std::string& Peer() const = 0;
};

using IChannelPtr = std::shared_ptr<IChannel>;

}  // namespace whirl::rpc
