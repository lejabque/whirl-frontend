#include <whirl/rpc/impl/client.hpp>

#include <whirl/rpc/impl/transport_channel.hpp>

namespace whirl::rpc {

class Client : public IClient {
 public:
  Client(ITransportPtr t, IExecutorPtr e) : t_(std::move(t)), e_(std::move(e)) {
  }

  IChannelPtr MakeChannel(const std::string& peer) override {
    return std::make_shared<TransportChannel>(t_, e_, peer);
  }

 private:
  ITransportPtr t_;
  IExecutorPtr e_;
};

IClientPtr MakeClient(ITransportPtr t, IExecutorPtr e) {
  return std::make_shared<Client>(t, e);
}

}  // namespace whirl::rpc
