#pragma once

#include "modules/audio_processing/api/AudioLevelSource.hpp"

#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <utility>

namespace ghub::fakes {

// Fonte de níveis falsa: o teste define os níveis e a falha de start(); conta start/stop.
class FakeAudioLevelSource final : public audio_processing::AudioLevelSource {
public:
    [[nodiscard]] shared_kernel::Result<void> start() override
    {
        ++starts_;
        if (startFailure_.has_value()) {
            return std::unexpected(*startFailure_);
        }
        running_ = true;
        return {};
    }
    void stop() override
    {
        ++stops_;
        running_ = false;
    }
    [[nodiscard]] bool running() const override { return running_; }
    [[nodiscard]] std::array<float, 10> levels() const override
    {
        const std::lock_guard lock{mutex_};
        return running_ ? levels_ : std::array<float, 10>{};
    }

    void setLevels(const std::array<float, 10>& levels)
    {
        const std::lock_guard lock{mutex_};
        levels_ = levels;
    }
    void failStart(shared_kernel::DomainError error) { startFailure_ = std::move(error); }
    [[nodiscard]] int starts() const noexcept { return starts_; }
    [[nodiscard]] int stops() const noexcept { return stops_; }

private:
    mutable std::mutex mutex_;
    std::array<float, 10> levels_{};
    std::optional<shared_kernel::DomainError> startFailure_;
    std::atomic<bool> running_{false};
    std::atomic<int> starts_{0};
    std::atomic<int> stops_{0};
};

} // namespace ghub::fakes
