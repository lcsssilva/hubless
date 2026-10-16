#pragma once

#include "modules/lighting/application/ScreenColorSource.hpp"

#include <atomic>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace ghub::fakes {

// Fonte de cores de tela falsa: o teste define as regiões e a falha de start(); conta start/stop. Nenhuma captura real.
class FakeScreenColorSource final : public lighting::application::ScreenColorSource {
public:
    [[nodiscard]] shared_kernel::Result<void> start() override
    {
        ++starts_;
        if (blockStart_) { // "esperando o usuário": só um stop() (cancelamento) solta
            stopped_ = false;
            insideStart_ = true;
            while (!stopped_) {
                std::this_thread::yield(); // espera por condição, sem dormir
            }
            insideStart_ = false;
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "cancelado");
        }
        if (startFailure_.has_value()) {
            return std::unexpected(*startFailure_);
        }
        running_ = true;
        return {};
    }
    void stop() override
    {
        ++stops_;
        stopped_ = true;
        running_ = false;
    }
    [[nodiscard]] bool running() const override { return running_; }
    [[nodiscard]] std::vector<lighting::domain::RgbColor> regions() const override
    {
        const std::lock_guard lock{mutex_};
        return running_ ? regions_ : std::vector<lighting::domain::RgbColor>{};
    }

    void setRegions(std::vector<lighting::domain::RgbColor> regions)
    {
        const std::lock_guard lock{mutex_};
        regions_ = std::move(regions);
    }
    void blockInStart() { blockStart_ = true; }
    [[nodiscard]] bool insideStart() const noexcept { return insideStart_; }
    void failStart(shared_kernel::DomainError error) { startFailure_ = std::move(error); }
    [[nodiscard]] int starts() const noexcept { return starts_; }
    [[nodiscard]] int stops() const noexcept { return stops_; }

private:
    mutable std::mutex mutex_;
    std::vector<lighting::domain::RgbColor> regions_;
    std::optional<shared_kernel::DomainError> startFailure_;
    std::atomic<bool> running_{false};
    std::atomic<bool> blockStart_{false};
    std::atomic<bool> insideStart_{false};
    std::atomic<bool> stopped_{false};
    std::atomic<int> starts_{0};
    std::atomic<int> stops_{0};
};

} // namespace ghub::fakes
