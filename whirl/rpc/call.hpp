#pragma once

#include <whirl/rpc/channel.hpp>
#include <whirl/rpc/bytes_value.hpp>
#include <whirl/rpc/method.hpp>
#include <whirl/rpc/input.hpp>

#include <whirl/cereal/serialize.hpp>

#include <await/futures/helpers.hpp>

#include <cereal/types/string.hpp>

#include <optional>

namespace whirl::rpc {

namespace detail {

using await::futures::Future;

template <typename T>
Future<T> As(Future<BytesValue> f_raw) {
  return std::move(f_raw).Then([](BytesValue raw) {
    return Deserialize<T>(raw);
  });
}

template <>
inline Future<void> As(Future<BytesValue> f_raw) {
  return await::futures::JustStatus(std::move(f_raw));
}

class [[nodiscard]] Caller1 {
 public:
  Caller1(Method method, BytesValue input, IChannelPtr channel)
      : method_(method),
        input_(std::move(input)),
        channel_(std::move(channel)),
        stop_token_(DefaultStopToken()) {
  }

  Caller1& StopAdvice(await::StopToken stop_token) {
    stop_token_ = std::move(stop_token);
    return *this;
  }

  Caller1& WithTraceId(TraceId trace_id) {
    trace_id_ = trace_id;
    return *this;
  }

  Caller1& LimitAttempts(size_t limit) {
    attempts_limit_ = limit;
    return *this;
  }

  template <typename T>
  Future<T> As() {
    return detail::As<T>(Call());
  }

  template <typename T>
  operator Future<T>() {
    return detail::As<T>(Call());
  }

 private:
  await::StopToken DefaultStopToken();
  TraceId GetTraceId();

  CallOptions MakeCallOptions() {
    return {GetTraceId(), stop_token_, attempts_limit_};
  }

  Future<BytesValue> Call() {
    return channel_->Call(method_, input_, MakeCallOptions());
  }

 private:
  Method method_;
  BytesValue input_;
  IChannelPtr channel_{nullptr};

  // Call context
  std::optional<TraceId> trace_id_;
  await::StopToken stop_token_;
  size_t attempts_limit_{0};  // 0 - Infinite
};

class [[nodiscard]] Caller0 {
 public:
  Caller0(Method method, BytesValue input)
      : method_(method), input_(std::move(input)) {
  }

  Caller1 Via(IChannelPtr channel) {
    return Caller1(std::move(method_), std::move(input_), std::move(channel));
  }

  Method method_;
  BytesValue input_;
};

}  // namespace detail

// Unary RPC
// Usage:
// auto f = Call("EchoService.Echo", proto::Echo::Request{data})
//            .Via(channel)
//            .StopAdvice(stop_token)
//            .WithTraceId(trace_id)
//            .LimitRetries(77)
//            .As<proto::Echo::Response>();
//
// .As<R>() is optional:
// Future<proto::Echo::Response> f =
//   Call("EchoService.Echo", proto::Echo::Request{data})
//      .Via(channel);

// TODO: BlockingCall

template <typename... Arguments>
detail::Caller0 Call(const std::string& method_str, Arguments&&... arguments) {
  auto method = Method::Parse(method_str);
  // Erase argument types
  auto input = detail::SerializeInput(std::forward<Arguments>(arguments)...);
  return detail::Caller0{method, input};
}

}  // namespace whirl::rpc
