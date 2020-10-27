#pragma once

#include <whirl/node/services.hpp>
#include <whirl/node/node.hpp>

#include <whirl/matrix/world/actor.hpp>
#include <whirl/matrix/world/faults.hpp>
#include <whirl/matrix/network/network.hpp>
#include <whirl/matrix/server/clocks.hpp>
#include <whirl/matrix/server/config.hpp>
#include <whirl/matrix/process/heap.hpp>
#include <whirl/matrix/process/network.hpp>
#include <whirl/matrix/common/event_queue.hpp>
#include <whirl/matrix/common/copy.hpp>
#include <whirl/matrix/common/hide_to_heap.hpp>

#include <whirl/matrix/server/services/local_storage.hpp>

#include <whirl/matrix/log/logger.hpp>

#include <memory>

namespace whirl {

//////////////////////////////////////////////////////////////////////

class Server : public IActor, public IFaultyServer {
 public:
  Server(Network& network, ServerConfig config, INodeFactoryPtr factory)
      : config_(config),
        node_factory_(std::move(factory)),
        network_(network, Name()) {
    Create();
  }

  // Non-copyable
  Server(const Server& that) = delete;
  Server& operator=(const Server& that) = delete;

  ~Server() {
    node_factory_.reset();
    Crash();
  }

  ServerAddress NetAddress() const {
    return Name();
  }

  // IFaultyServer

  void Reboot() override {
    Crash();
    Create();
    Start();
  }

  void Pause() override {
    WHEELS_VERIFY(!paused_, "Server already paused");
    paused_ = true;
  }

  void Resume() override {
    WHEELS_VERIFY(paused_, "Server is not paused");

    auto now = GlobalNow();
    auto g = heap_.Use();

    while (!events_.IsEmpty() && events_.NextEventTime() < now) {
      auto pending_event = events_.TakeNext();
      events_.Add(now, std::move(pending_event.action));
    }

    paused_ = false;
  }

  void AdjustWallClock() override {
    wall_clock_.AdjustOffset();
  }

  // IActor

  void Start() override {
    WHIRL_LOG("Start node at server " << Name());

    auto g = heap_.Use();

    auto node = std::move(node_);
    node->Start();
    HideToHeap(std::move(node));
  }

  const std::string& Name() const override {
    return config_.name;
  }

  bool IsRunnable() const override {
    if (paused_) {
      return false;
    }
    auto g = heap_.Use();
    return !events_.IsEmpty();
  }

  TimePoint NextStepTime() override {
    auto g = heap_.Use();
    return events_.NextEventTime();
  }

  void Step() override {
    auto g = heap_.Use();
    auto event = events_.TakeNext();
    event();
  }

  void Shutdown() override {
    Crash();
  }

 private:
  NodeServices CreateNodeServices();

  void Create() {
    monotonic_clock_.Reset();

    auto g = heap_.Use();
    auto services = CreateNodeServices();
    node_ = node_factory_->CreateNode(std::move(services));
  }

  void Crash() {
    WHIRL_LOG("Crash server " << Name());

    // Reset all client connections
    network_.Reset();

    WHIRL_LOG("Bytes allocated on process heap: " << heap_.BytesAllocated());
    {
      auto g = heap_.Use();
      events_.Clear();
    }
    heap_.Reset();

    paused_ = false;
  }

 private:
  ServerConfig config_;
  INodeFactoryPtr node_factory_;
  std::string name_;

  ProcessNetwork network_;

  LocalWallClock wall_clock_;
  LocalMonotonicClock monotonic_clock_;
  LocalBytesStorage storage_;

  // Node process

  mutable ProcessHeap heap_;
  EventQueue events_;
  bool paused_{false};  // TODO: better?

  INodePtr node_;

  Logger logger_{"Server"};
};

//////////////////////////////////////////////////////////////////////

}  // namespace whirl
