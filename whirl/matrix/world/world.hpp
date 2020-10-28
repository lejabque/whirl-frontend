#pragma once

#include <whirl/matrix/world/behaviour.hpp>
#include <whirl/matrix/adversary/strategy.hpp>
#include <whirl/matrix/history/history.hpp>
#include <whirl/node/node.hpp>

#include <memory>
#include <ostream>
#include <any>

namespace whirl {

class WorldImpl;

// Facade

class World {
  static const size_t kDefaultSeed = 42;

 public:
  World(size_t seed = kDefaultSeed);
  ~World();

  void AddServer(INodeFactoryPtr node);
  void AddServers(size_t count, INodeFactoryPtr node);

  void AddClient(INodeFactoryPtr node);
  void AddClients(size_t count, INodeFactoryPtr node);

  void SetBehaviour(IWorldBehaviourPtr behaviour);

  void SetAdversary(adversary::Strategy strategy);

  template <typename T>
  void SetGlobal(const std::string& key, T value) {
    SetGlobalImpl(key, value);
  }

  void WriteLogTo(std::ostream& out);

  void Start();

  bool Step();
  void MakeSteps(size_t count);

  // Returns hash of simulation
  size_t Stop();

  size_t NumCompletedCalls() const;

  Duration TimeElapsed() const;

  const histories::History& History() const;

 private:
  void SetGlobalImpl(const std::string& key, std::any value);

 private:
  std::unique_ptr<WorldImpl> impl_;
};

}  // namespace whirl
