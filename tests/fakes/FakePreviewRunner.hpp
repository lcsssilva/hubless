#pragma once

#include "modules/camera/application/PreviewRunner.hpp"

#include <vector>

namespace ghub::fakes {

// Runner falso: registra os modos pedidos em `start` e conta as chamadas a `stop`.
class FakePreviewRunner final : public camera::PreviewRunner {
public:
    void start(const camera::PreviewMode& mode) override { startedModes_.push_back(mode); }
    void stop() override { ++stopCalls_; }

    [[nodiscard]] const std::vector<camera::PreviewMode>& startedModes() const noexcept { return startedModes_; }
    [[nodiscard]] int stopCalls() const noexcept { return stopCalls_; }

private:
    std::vector<camera::PreviewMode> startedModes_;
    int stopCalls_ = 0;
};

} // namespace ghub::fakes
