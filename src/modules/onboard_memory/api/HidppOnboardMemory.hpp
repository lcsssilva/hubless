#pragma once

#include "modules/onboard_memory/application/OnboardMemory.hpp"
#include "shared_kernel/Clock.hpp"

#include <cstdint>
#include <memory>
#include <span>

// Forward declaration: o consumidor inclui platform/hidpp/HidReportIo.hpp na própria infraestrutura.
namespace ghub::platform::hidpp {
class HidReportIo;
}

namespace ghub::onboard_memory {

// Política de comandos HID++ de um dispositivo com memória onboard (0x8100), injetada pelo módulo dono do
// dispositivo (ADR-0027 decisão 3). `isAllowed` tem a mesma assinatura de platform::hidpp::HidppCommandPolicy, de
// modo que uma única classe pode implementar as duas interfaces. O adaptador chama `setProfileCount(0)` ao ser
// criado e só informa a contagem real depois de um describe() bem-sucedido.
class OnboardProfilePolicy {
public:
    virtual ~OnboardProfilePolicy() = default;

    [[nodiscard]] virtual bool isAllowed(std::uint16_t featureId, std::uint8_t function,
                                         std::span<const std::uint8_t> params) const noexcept = 0;
    virtual void setProfileCount(std::uint8_t profileCount) noexcept = 0;

protected:
    OnboardProfilePolicy() = default;
    OnboardProfilePolicy(const OnboardProfilePolicy&) = default;
    OnboardProfilePolicy& operator=(const OnboardProfilePolicy&) = default;
    OnboardProfilePolicy(OnboardProfilePolicy&&) = default;
    OnboardProfilePolicy& operator=(OnboardProfilePolicy&&) = default;
};

// Memória onboard sobre o canal HID++ 2.0 (feature 0x8100). Qualquer erro encerra a operação na hora, sem
// retentativa. O relógio precisa sobreviver à memória devolvida.
[[nodiscard]] std::unique_ptr<OnboardMemory> makeHidppOnboardMemory(std::unique_ptr<platform::hidpp::HidReportIo> io,
                                                                    const shared_kernel::Clock& clock,
                                                                    std::uint8_t deviceIndex,
                                                                    std::unique_ptr<OnboardProfilePolicy> policy);

} // namespace ghub::onboard_memory
