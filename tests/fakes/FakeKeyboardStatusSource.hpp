#pragma once

#include "modules/lighting/application/KeyboardStatusSource.hpp"

#include <functional>
#include <mutex>
#include <utility>

namespace ghub::fakes {

// Fonte de estado do teclado que conta leituras e devolve o resultado configurado. Segura para thread de trabalho.
// `onRead` (opcional) roda dentro de cada leitura, antes de devolver.
class FakeKeyboardStatusSource final : public lighting::application::KeyboardStatusSource {
public:
    [[nodiscard]] shared_kernel::Result<lighting::KeyboardStatus> read() override
    {
        std::function<void()> hook;
        {
            const std::scoped_lock lock{mutex_};
            ++reads_;
            hook = onRead_;
        }
        if (hook) {
            hook();
        }
        const std::scoped_lock lock{mutex_};
        return result_;
    }

    void setResult(shared_kernel::Result<lighting::KeyboardStatus> result)
    {
        const std::scoped_lock lock{mutex_};
        result_ = std::move(result);
    }
    void setOnRead(std::function<void()> hook)
    {
        const std::scoped_lock lock{mutex_};
        onRead_ = std::move(hook);
    }
    [[nodiscard]] int reads() const
    {
        const std::scoped_lock lock{mutex_};
        return reads_;
    }

private:
    mutable std::mutex mutex_;
    shared_kernel::Result<lighting::KeyboardStatus> result_{lighting::KeyboardStatus{}};
    std::function<void()> onRead_;
    int reads_ = 0;
};

} // namespace ghub::fakes
