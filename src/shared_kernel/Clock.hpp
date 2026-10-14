#pragma once

#include <chrono>

namespace ghub::shared_kernel {

using Instant = std::chrono::steady_clock::time_point;
using Duration = std::chrono::steady_clock::duration;

// Port de tempo monotônico; o domínio nunca lê o relógio do sistema diretamente.
// Implementações devem ser seguras para chamada concorrente.
class Clock {
public:
    virtual ~Clock() = default;
    [[nodiscard]] virtual Instant now() const = 0;

protected:
    Clock() = default;
    Clock(const Clock&) = default;
    Clock& operator=(const Clock&) = default;
    Clock(Clock&&) = default;
    Clock& operator=(Clock&&) = default;
};

} // namespace ghub::shared_kernel
