#pragma once

#include "shared_kernel/Clock.hpp"

#include <atomic>

namespace ghub::fakes {

// Relógio controlado pelo teste; avança somente quando advance() é chamado.
class FakeClock final : public shared_kernel::Clock {
public:
    [[nodiscard]] shared_kernel::Instant now() const override { return now_.load(); }

    void advance(shared_kernel::Duration delta) { now_.store(now_.load() + delta); }

private:
    std::atomic<shared_kernel::Instant> now_{shared_kernel::Instant{}};
};

} // namespace ghub::fakes
