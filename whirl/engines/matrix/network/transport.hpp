#pragma once

#include <whirl/engines/matrix/network/address.hpp>
#include <whirl/engines/matrix/network/packet.hpp>
#include <whirl/engines/matrix/network/timestamp.hpp>
#include <whirl/engines/matrix/network/socket.hpp>

#include <whirl/engines/matrix/process/heap.hpp>
#include <whirl/engines/matrix/process/scheduler.hpp>

#include <whirl/logger/log.hpp>

#include <map>

namespace whirl::matrix::net {

//////////////////////////////////////////////////////////////////////

class Network;
class Link;

//////////////////////////////////////////////////////////////////////

struct ISocketHandler {
  virtual ~ISocketHandler() = default;

  virtual void HandleMessage(const Message& message, ReplySocket back) = 0;

  virtual void HandleDisconnect(const std::string& peer) = 0;
};

//////////////////////////////////////////////////////////////////////

// ~ TCP, Per-server
class Transport {
  struct Endpoint {
    ISocketHandler* handler;
    Timestamp ts;
  };

  friend class ClientSocket;
  friend class ServerSocket;

 public:
  Transport(Network& net, const std::string& host, ProcessHeap& heap, TaskScheduler& scheduler);

  // Context: Server
  ClientSocket ConnectTo(const Address& address, ISocketHandler* handler);

  // Context: Server
  ServerSocket Serve(Port port, ISocketHandler* handler);

  // Context: Network
  void HandlePacket(const Packet& packet, Link* out);

  // On server crash
  void Reset();

 private:
  // Context: Server
  // Invoked from socket destructor
  void RemoveEndpoint(Port port);

  Port FindFreePort();

 private:
  Network& net_;
  std::string host_;

  // Local endpoints
  std::map<Port, Endpoint> endpoints_;

  Port next_port_{1};

  // To invoke ISocketHandler methods
  ProcessHeap& heap_;
  TaskScheduler& scheduler_;

  Logger logger_{"Transport"};
};

}  // namespace whirl::matrix::net
