#include <whirl/matrix/channels/logging.hpp>

#include <whirl/rpc/impl/channel.hpp>

#include <whirl/matrix/log/logger.hpp>

#include <await/futures/promise.hpp>
#include <await/futures/helpers.hpp>

#include <vector>

namespace whirl {

static Logger logger_{"Logging channel"};

using namespace rpc;
using wheels::Result;

class LoggingChannel : public rpc::IChannel {
 public:
  LoggingChannel(IChannelPtr impl) : impl_(std::move(impl)) {
  }

  void Close() override {
    impl_->Close();
  }

  const std::string& Peer() const override {
    return impl_->Peer();
  }

  Future<BytesValue> Call(const Method& method,
                          const BytesValue& input) override {
    auto f = impl_->Call(method, input);

    auto log = [method,
                peer = Peer()](const Result<BytesValue>& result) mutable {
      if (result.IsOk()) {
        WHIRL_FMT_LOG("Call {}.{} completed: Ok", peer, method);
      } else {
        WHIRL_FMT_LOG("Call {}.{} failed: {}", peer, method,
                      result.GetErrorCode().message());
      }
    };

    return await::futures::SubscribeConst(std::move(f), std::move(log));
  }

 private:
  rpc::IChannelPtr impl_;
};

rpc::IChannelPtr MakeLoggingChannel(rpc::IChannelPtr channel) {
  return std::make_shared<LoggingChannel>(std::move(channel));
}

}  // namespace whirl
