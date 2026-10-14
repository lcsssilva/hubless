#pragma once

#include "modules/pointer/application/OnboardMemory.hpp"
#include "modules/pointer/infrastructure/G703OnboardPolicy.hpp"
#include "platform/hidpp/HidReportIo.hpp"
#include "platform/hidpp/HidppChannel.hpp"
#include "shared_kernel/Clock.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace ghub::pointer::infrastructure {

// Adaptador do port OnboardMemory sobre o canal HID++ 2.0 (feature 0x8100, ADR-0017). Possui a G703OnboardPolicy
// e o canal; a política começa com profileCount 0 (nenhuma escrita) e só passa a aceitar setores de perfil depois
// de um describe() bem-sucedido. Qualquer erro encerra a operação na hora: nenhum relatório adicional e nenhuma
// retentativa. Não é seguro para threads; o DeviceLease garante o escritor único.
class HidppOnboardMemory final : public OnboardMemory {
public:
    HidppOnboardMemory(std::unique_ptr<platform::hidpp::HidReportIo> io, const shared_kernel::Clock& clock,
                       std::uint8_t deviceIndex);

    [[nodiscard]] shared_kernel::Result<OnboardLayout> describe() override;
    [[nodiscard]] shared_kernel::Result<std::uint8_t> onboardMode() override;
    [[nodiscard]] shared_kernel::Result<std::uint16_t> currentProfileSector() override;
    [[nodiscard]] shared_kernel::Result<std::vector<std::uint8_t>> readSector(std::uint16_t sector,
                                                                              std::uint16_t size) override;
    // Exige describe() prévio e bytes.size() == sectorSize descrito. fn6 + ceil(n/16) fn7 + fn8.
    [[nodiscard]] shared_kernel::Result<void> writeSector(std::uint16_t sector,
                                                          std::span<const std::uint8_t> bytes) override;

private:
    [[nodiscard]] shared_kernel::Result<platform::hidpp::HidppChannel::Response>
    call(std::uint8_t function, std::span<const std::uint8_t> params = {});

    // A política precisa ser declarada antes do canal: o canal guarda uma referência a ela.
    G703OnboardPolicy policy_;
    platform::hidpp::HidppChannel channel_;
    std::uint8_t deviceIndex_;
    std::optional<OnboardLayout> layout_;
};

} // namespace ghub::pointer::infrastructure
