#pragma once

#include "modules/pointer/application/PointerReadRunner.hpp"

#include <optional>
#include <utility>

namespace ghub::fakes {

// Runner falso: conta as chamadas a `start` e devolve o estado programado pelo teste.
class FakePointerReadRunner final : public pointer::PointerReadRunner {
public:
    void start() override { ++startCalls_; }
    [[nodiscard]] bool isReading() const override { return reading_; }
    [[nodiscard]] std::optional<pointer::PointerStatus> lastStatus() const override { return status_; }

    void setReading(bool reading) noexcept { reading_ = reading; }
    void setStatus(std::optional<pointer::PointerStatus> status) { status_ = std::move(status); }

    [[nodiscard]] int startCalls() const noexcept { return startCalls_; }

private:
    int startCalls_ = 0;
    bool reading_ = false;
    std::optional<pointer::PointerStatus> status_;
};

} // namespace ghub::fakes
