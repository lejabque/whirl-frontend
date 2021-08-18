#pragma once

#include <whirl/rpc/service.hpp>
#include <whirl/rpc/invoke_helper.hpp>

#include <wheels/support/preprocessor.hpp>
#include <wheels/support/assert.hpp>

#include <map>

namespace whirl::rpc {

//////////////////////////////////////////////////////////////////////

template <typename TService>
class ServiceBase : public IService {
 protected:
  // See RPC_REGISTER_METHOD macro
  using ThisService = TService;

  using MethodName = std::string;

 public:
  bool Has(const MethodName& name) const override {
    return methods_.find(name) != methods_.end();
  }

  BytesValue Invoke(const MethodName& name, const BytesValue& input) override {
    auto method_it = methods_.find(name);
    WHEELS_VERIFY(method_it != methods_.end(),
                  "RPC method not found: " << name);

    auto& invoker = method_it->second;
    return invoker(input);
  }

  void Initialize() override {
    RegisterRPCMethods();
  }

 protected:
  // Override this
  virtual void RegisterRPCMethods() {
    // Use RPC_REGISTER_METHOD macro
  }

  template <typename R, typename... Args>
  void RegisterRPCMethod(const MethodName& name,
                         R (TService::*method)(Args...)) {
    TService* self = static_cast<TService*>(this);

    auto closure = [self, method](Args&&... args) -> R {
      return (self->*method)(std::forward<Args>(args)...);
    };

    auto invoker = [closure = std::move(closure)](
                       const BytesValue& input) mutable -> BytesValue {
      return detail::InvokeHelper<R, Args...>::Invoke(closure, input);
    };

    methods_.emplace(name, invoker);
  }

 private:
  using MethodInvoker = std::function<BytesValue(BytesValue)>;

  std::map<MethodName, MethodInvoker> methods_;
};

}  // namespace whirl::rpc

//////////////////////////////////////////////////////////////////////

#define WHIRL_RPC_REGISTER_METHOD(method) \
  RegisterRPCMethod(TO_STRING(method), &ThisService::method)
