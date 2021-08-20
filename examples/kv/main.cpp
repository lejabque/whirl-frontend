#include <whirl/program/main.hpp>
#include <whirl/peer/peer.hpp>
#include <whirl/db/store/kv.hpp>
#include <whirl/logger/log.hpp>
#include <whirl/rpc/service_base.hpp>
#include <whirl/rpc/call.hpp>
#include <whirl/cereal/serializable.hpp>

#include <whirl/runtime/methods.hpp>

// Simulation
#include <whirl/engines/matrix/world/world.hpp>
#include <whirl/engines/matrix/world/global/vars.hpp>
#include <whirl/engines/matrix/client/main.hpp>
#include <whirl/engines/matrix/client/rpc.hpp>
#include <whirl/engines/matrix/client/util.hpp>
#include <whirl/engines/matrix/test/random.hpp>
#include <whirl/engines/matrix/test/main.hpp>
#include <whirl/engines/matrix/test/event_log.hpp>
#include <whirl/engines/matrix/memory/new.hpp>

#include <whirl/engines/matrix/fault/accessors.hpp>
#include <whirl/engines/matrix/fault/net/star.hpp>

#include <whirl/history/printers/kv.hpp>
#include <whirl/history/checker/check.hpp>
#include <whirl/history/models/kv.hpp>

#include <await/fibers/core/id.hpp>
#include <whirl/rpc/id.hpp>

// Concurrency
#include <await/fibers/sync/future.hpp>
#include <await/fibers/core/await.hpp>
#include <await/fibers/sync/mutex.hpp>
#include <await/futures/combine/quorum.hpp>

// Support std::string serialization
#include <cereal/types/string.hpp>

#include <fmt/ostream.h>

#include <random>
#include <algorithm>

using await::futures::Future;
using wheels::Result;
using namespace await::fibers;
using namespace whirl;
using namespace whirl::time_literals;

//////////////////////////////////////////////////////////////////////

// string -> int
using Key = std::string;
using Value = int32_t;

//////////////////////////////////////////////////////////////////////

// Replicas store versioned (stamped) values

using Timestamp = size_t;

struct StampedValue {
  Value value;
  Timestamp timestamp;

  static StampedValue NoValue() {
    return {0, 0};
  }

  // Serialization support for local storage and RPC
  WHIRL_SERIALIZABLE(value, timestamp)
};

// For logging
std::ostream& operator<<(std::ostream& out, const StampedValue& stamped_value) {
  out << "{" << stamped_value.value << ", ts: " << stamped_value.timestamp
      << "}";
  return out;
}

//////////////////////////////////////////////////////////////////////

// KV storage / bunch of atomic R/W registers

// RPC services / algorithm roles

// Coordinator role, stateless

class Coordinator : public rpc::ServiceBase<Coordinator>, public node::Peer {
 public:
  Coordinator() : Peer(node::rt::Config()->PoolName()) {
  }

  void RegisterMethods() override {
    WHIRL_RPC_REGISTER_METHOD(Set);
    WHIRL_RPC_REGISTER_METHOD(Get);
  };

  // RPC handlers

  void Set(Key key, Value value) {
    Timestamp write_ts = ChooseWriteTimestamp();
    WHIRL_LOG_INFO("Write timestamp: {}", write_ts);

    std::vector<Future<void>> writes;

    // Broadcast
    for (const auto& peer : ListPeers(/*with_me=*/true)) {
      writes.push_back(  //
          rpc::Call("Replica.LocalWrite")
              .Args<Key, StampedValue>(key, {value, write_ts})
              .Via(Channel(peer))
              .AtLeastOnce());
    }

    // Await acknowledgements from the majority of storage replicas
    Await(Quorum(std::move(writes), /*threshold=*/Majority())).ThrowIfError();
  }

  Value Get(Key key) {
    std::vector<Future<StampedValue>> reads;

    // Broadcast LocalRead request to replicas
    for (const auto& peer : ListPeers(/*with_me=*/true)) {
      reads.push_back(  //
          rpc::Call("Replica.LocalRead")
              .Args(key)
              .Via(Channel(peer))
              .AtLeastOnce());
    }

    // Await responses from the majority of replicas

    // 1) Combine futures from read RPC-s to single quorum future
    Future<std::vector<StampedValue>> quorum_reads =
        Quorum(std::move(reads), /*threshold=*/Majority());
    // 2) Block current fiber until quorum collected
    Result<std::vector<StampedValue>> results = Await(std::move(quorum_reads));
    // 3) Unpack vector or throw error
    std::vector<StampedValue> stamped_values = results.ValueOrThrow();

    // Or combine all steps into:
    // auto stamped_values = Await(Quorum(std::move(reads),
    // Majority())).ValueOrThrow()

    for (size_t i = 0; i < stamped_values.size(); ++i) {
      WHIRL_LOG_INFO("{}-th value in read quorum: {}", i + 1,
                     stamped_values[i]);
    }

    auto most_recent = FindMostRecent(stamped_values);
    return most_recent.value;
  }

 private:
  Timestamp ChooseWriteTimestamp() const {
    // Local wall clock may be out of sync with other replicas
    // Use TrueTime (TrueTime() method)
    return node::rt::WallTimeNow();
  }

  // Find value with the largest timestamp
  StampedValue FindMostRecent(const std::vector<StampedValue>& values) const {
    return *std::max_element(
        values.begin(), values.end(),
        [](const StampedValue& lhs, const StampedValue& rhs) {
          return lhs.timestamp < rhs.timestamp;
        });
  }

  // Quorum size
  size_t Majority() const {
    return NodeCount() / 2 + 1;
  }

 private:
  Logger logger_{"KVNode.Coordinator"};
};

// Storage replica role

class Replica : public rpc::ServiceBase<Replica> {
 public:
  Replica() : kv_store_(node::rt::Database(), "abd") {
  }

  void RegisterMethods() override {
    WHIRL_RPC_REGISTER_METHOD(LocalWrite);
    WHIRL_RPC_REGISTER_METHOD(LocalRead);
  };

  // RPC handlers

  void LocalWrite(Key key, StampedValue target_value) {
    std::lock_guard guard(write_mutex_);

    std::optional<StampedValue> local_value = kv_store_.TryGet(key);

    if (!local_value.has_value()) {
      // First write for this key
      Update(key, target_value);
    } else {
      // Write timestamp > timestamp of locally stored value
      if (target_value.timestamp > local_value->timestamp) {
        Update(key, target_value);
      }
    }
  }

  StampedValue LocalRead(Key key) {
    return kv_store_.GetOr(key, StampedValue::NoValue());
  }

 private:
  void Update(Key key, StampedValue target_value) {
    WHIRL_LOG_INFO("Write '{}' -> {}", key, target_value);
    kv_store_.Put(key, target_value);
  }

 private:
  // Local persistent K/V storage
  // strings -> StampedValues
  node::db::KVStore<StampedValue> kv_store_;
  // Mutex for _fibers_
  // Guards writes to kv_store_
  await::fibers::Mutex write_mutex_;

  Logger logger_{"KVNode.Replica"};
};

void KVNode() {
  node::main::Prologue();

  auto rpc_server = node::rt::MakeRpcServer();

  rpc_server->RegisterService("KV", std::make_shared<Coordinator>());
  rpc_server->RegisterService("Replica", std::make_shared<Replica>());

  rpc_server->Start();

  node::main::BlockForever();
}

//////////////////////////////////////////////////////////////////////

class KVBlockingStub {
 public:
  KVBlockingStub(rpc::IChannelPtr channel) : channel_(channel) {
  }

  void Set(Key k, Value v) {
    Await(rpc::Call("KV.Set").Args(k, v).Via(channel_).Start().As<void>())
        .ThrowIfError();
  }

  Value Get(Key k) {
    return Await(rpc::Call("KV.Get").Args(k).Via(channel_).Start().As<Value>())
        .ValueOrThrow();
  }

 private:
  rpc::IChannelPtr channel_;
};

//////////////////////////////////////////////////////////////////////

static const std::vector<std::string> kKeys({"a", "b", "c"});

const std::string& ChooseRandomKey() {
  return kKeys.at(node::rt::RandomNumber(matrix::GetGlobal<size_t>("keys")));
}

//////////////////////////////////////////////////////////////////////

[[noreturn]] void Client() {
  matrix::client::Prologue();

  Logger logger_{"Client"};

  KVBlockingStub kv_store{matrix::client::MakeRpcChannel(/*pool_name=*/ "kv")};

  for (size_t i = 1;; ++i) {
    Key key = ChooseRandomKey();
    if (matrix::client::Either()) {
      Value value = node::rt::RandomNumber(1, 100);
      WHIRL_LOG_INFO("Execute Put({}, {})", key, value);
      kv_store.Set(key, value);
      WHIRL_LOG_INFO("Put completed");
    } else {
      WHIRL_LOG_INFO("Execute Get({})", key);
      [[maybe_unused]] Value result = kv_store.Get(key);
      WHIRL_LOG_INFO("Get({}) -> {}", key, result);
    }

    matrix::GlobalCounter("requests").Increment();

    // Random pause
    node::rt::SleepFor(node::rt::RandomNumber(1, 100));
  }
}

//////////////////////////////////////////////////////////////////////

[[noreturn]] void Adversary() {
  Logger logger_{"Adversary"};

  // List system nodes
  auto pool = node::rt::Dns()->ListPool("kv");

  auto& net = matrix::fault::Network();

  while (true) {
    node::rt::SleepFor(
        node::rt::RandomNumber(10, 1000));

    size_t center = node::rt::RandomNumber(pool.size());

    WHIRL_LOG_INFO("Make star with center at {}", pool[center]);

    matrix::fault::MakeStar(pool, center);

    node::rt::SleepFor(
        node::rt::RandomNumber(100, 300));

    net.Heal();
  }
}

//////////////////////////////////////////////////////////////////////

// Sequential specification for KV storage
// Used by linearizability checker
using KVStoreModel = histories::KVStoreModel<Key, Value>;

//////////////////////////////////////////////////////////////////////

void FailTest() {
  std::cout << "(ﾉಥ益ಥ）ﾉ ┻━┻" << std::endl;
  std::cout.flush();
  std::exit(1);
}

//////////////////////////////////////////////////////////////////////

// Seed -> simulation digest
size_t RunSimulation(size_t seed) {
  static const size_t kTimeLimit = 10000;
  static const size_t kRequestsThreshold = 7;

  matrix::Random random{seed};

  // Randomize simulation parameters
  const size_t replicas = random.Get(3, 5);
  const size_t clients = random.Get(2, 3);
  const size_t keys = random.Get(1, 2);

  std::cout << "Simulation seed: " << seed << std::endl;

  std::cout << "Parameters: "
            << "replicas = " << replicas << ", "
            << "clients = " << clients << ", "
            << "keys = " << keys << std::endl;

  // Reset RPC and fiber ids
  await::fibers::ResetIds();
  whirl::rpc::ResetIds();

  matrix::World world{seed};

  // Cluster
  world.MakePool(
      /*pool_name=*/ "kv",
      /*program=*/ KVNode,
      /*size=*/ replicas,
      /*name_template=*/ "Server");

  // Clients
  world.AddClients(Client, /*count=*/ clients);

  world.SetAdversary(Adversary);

  // Globals
  world.SetGlobal("keys", keys);
  world.InitCounter("requests", 0);

  // Run simulation
  world.Start();
  while (world.GetCounter("requests") < kRequestsThreshold &&
         world.TimeElapsed() < kTimeLimit) {
    if (!world.Step()) {
      break;  // Deadlock
    }
  }

  // Stop and compute simulation digest
  size_t digest = world.Stop();

  // Print report
  std::cout << "Seed " << seed << " -> "
            << "digest: " << digest << ", time: " << world.TimeElapsed()
            << ", steps: " << world.StepCount() << std::endl;

  const auto event_log = world.EventLog();

  // Time limit exceeded
  if (world.GetCounter("requests") < kRequestsThreshold) {
    // Log
    std::cout << "Log:" << std::endl;
    matrix::WriteTextLog(event_log, std::cout);
    std::cout << std::endl;

    if (world.TimeElapsed() < kTimeLimit) {
      std::cout << "Deadlock in simulation" << std::endl;
    } else {
      std::cout << "Simulation time limit exceeded" << std::endl;
    }
    std::exit(1);
  }

  std::cout << "Requests completed: " << world.GetCounter("requests")
            << std::endl;

  // Check linearizability
  const auto history = world.History();
  const bool linearizable = histories::LinCheck<KVStoreModel>(history);

  if (!linearizable) {
    // Log
    std::cout << "Log:" << std::endl;
    matrix::WriteTextLog(event_log, std::cout);
    std::cout << std::endl;

    // History
    std::cout << "History (seed = " << seed
              << ") is NOT LINEARIZABLE:" << std::endl;
    histories::PrintKVHistory<Key, Value>(history, std::cout);

    FailTest();
  }

  return digest;
}

// Usage:
// 1) --det --sims 12345 - check determinism and run 12345 simulations
// 2) --seed 54321 - run single simulation with seed 54321

int main(int argc, const char** argv) {
  return matrix::Main(argc, argv, RunSimulation);
}
