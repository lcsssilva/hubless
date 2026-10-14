#pragma once

#include "modules/pointer/application/PointerBackend.hpp"

#include <atomic>
#include <functional>
#include <utility>
#include <vector>

namespace ghub::fakes {

// Backend de mouse que devolve o resultado programado pelo teste, conta as leituras e permite observar
// o estado do mundo durante a leitura (`onRead`). `calls()` pode ser lido de outra thread.
class FakePointerBackend final : public pointer::PointerBackend {
public:
    [[nodiscard]] shared_kernel::Result<std::vector<pointer::DiscoveredPointer>> readDevices() override
    {
        calls_.fetch_add(1);
        if (onRead_) {
            onRead_();
        }
        return result_;
    }

    void setResult(shared_kernel::Result<std::vector<pointer::DiscoveredPointer>> result)
    {
        result_ = std::move(result);
    }

    void setOnRead(std::function<void()> callback) { onRead_ = std::move(callback); }

    [[nodiscard]] int calls() const noexcept { return calls_.load(); }

private:
    shared_kernel::Result<std::vector<pointer::DiscoveredPointer>> result_;
    std::function<void()> onRead_;
    std::atomic<int> calls_{0};
};

} // namespace ghub::fakes
