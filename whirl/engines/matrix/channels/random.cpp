#include <whirl/engines/matrix/channels/random.hpp>

#include <whirl/engines/matrix/world/global/random.hpp>

using namespace whirl::rpc;
using await::util::StopToken;

namespace whirl::matrix {

using ChannelVector = std::vector<IChannelPtr>;

static const std::string kRandomPeer = "Random";

class RandomChannel : public IChannel {
 public:
  RandomChannel(ChannelVector channels) : channels_(std::move(channels)) {
  }

  ~RandomChannel() {
    Close();
  }

  Future<BytesValue> Call(const Method& method,
                          const BytesValue& input,
                          CallContext ctx) override {
    size_t index = GlobalRandomNumber(channels_.size());
    return channels_[index]->Call(method, input, std::move(ctx));
  }

  const std::string& Peer() const override {
    return kRandomPeer;
  }

  void Close() override {
    for (auto& channel : channels_) {
      channel->Close();
    }
    channels_.clear();
  }

 private:
  ChannelVector channels_;
};

IChannelPtr MakeRandomChannel(ChannelVector&& channels) {
  return std::make_shared<RandomChannel>(std::move(channels));
}

}  // namespace whirl::matrix