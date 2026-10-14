#pragma once

#include "shared_kernel/Clock.hpp"

namespace ghub::device_access {

// Relógio de produção do port Clock: std::chrono::steady_clock (monotônico, seguro entre threads).
class SteadyClock final : public shared_kernel::Clock {
public:
    [[nodiscard]] shared_kernel::Instant now() const override;
};

} // namespace ghub::device_access
