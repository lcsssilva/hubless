#pragma once

#include "modules/headset/application/HeadsetSettingsWriter.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace ghub::fakes {

// Writer de headset que conta chamadas e devolve o resultado configurado pelo teste. Seguro para ser chamado
// de uma thread de trabalho enquanto o teste lê os contadores. `onCall` (opcional) roda dentro de cada chamada,
// antes de devolver (ex.: para bloquear a thread até o teste liberar).
class FakeHeadsetSettingsWriter final : public headset::HeadsetSettingsWriter {
public:
    [[nodiscard]] shared_kernel::Result<void> applySidetone(headset::SidetoneLevel level) override
    {
        record(level.value(), -1);
        return result();
    }

    [[nodiscard]] shared_kernel::Result<void> applyLights(headset::HeadsetLights lights) override
    {
        record(-1, lights == headset::HeadsetLights::On ? 1 : 0);
        return result();
    }

    void setResult(shared_kernel::Result<void> result)
    {
        const std::scoped_lock lock{mutex_};
        result_ = std::move(result);
    }

    void setOnCall(std::function<void()> hook)
    {
        const std::scoped_lock lock{mutex_};
        onCall_ = std::move(hook);
    }

    [[nodiscard]] int calls() const { return calls_.load(); }
    [[nodiscard]] int sidetoneCalls() const { return sidetoneCalls_.load(); }
    [[nodiscard]] int lightsCalls() const { return lightsCalls_.load(); }

    // Valores de sidetone aplicados, na ordem.
    [[nodiscard]] std::vector<int> sidetoneValues() const
    {
        const std::scoped_lock lock{mutex_};
        return sidetoneValues_;
    }

    // Estados de luz aplicados na ordem (true = ligar).
    [[nodiscard]] std::vector<bool> lightsValues() const
    {
        const std::scoped_lock lock{mutex_};
        return lightsValues_;
    }

private:
    void record(int sidetone, int lights)
    {
        std::function<void()> hook;
        {
            const std::scoped_lock lock{mutex_};
            if (sidetone >= 0) {
                sidetoneValues_.push_back(sidetone);
            } else {
                lightsValues_.push_back(lights != 0);
            }
            hook = onCall_;
        }
        ++calls_;
        if (sidetone >= 0) {
            ++sidetoneCalls_;
        } else {
            ++lightsCalls_;
        }
        if (hook) {
            hook();
        }
    }

    [[nodiscard]] shared_kernel::Result<void> result() const
    {
        const std::scoped_lock lock{mutex_};
        return result_;
    }

    mutable std::mutex mutex_;
    shared_kernel::Result<void> result_;
    std::function<void()> onCall_;
    std::vector<int> sidetoneValues_;
    std::vector<bool> lightsValues_;
    std::atomic<int> calls_{0};
    std::atomic<int> sidetoneCalls_{0};
    std::atomic<int> lightsCalls_{0};
};

} // namespace ghub::fakes
