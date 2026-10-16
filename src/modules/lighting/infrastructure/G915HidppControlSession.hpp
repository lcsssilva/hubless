#pragma once

#include "modules/lighting/application/KeyboardControl.hpp"
#include "platform/hidpp/HidppChannel.hpp"

#include <cstdint>

namespace ghub::lighting::infrastructure {

// Sessão HID++ 2.0 com o G915 para o modo de jogo (feature do ADR-0029, fn3/fn1) e a troca de slot (0x8100 fn3), mais a
// leitura do diretório de perfis (0x8100 fn5 do setor 0 e fn4) (INC-019, ADR-0029). Bytes conferidos com a captura C-04
// de 2026-10-15. `channel` precisa sobreviver à sessão; a política do canal é a LightingHidppPolicy. Não é segura para
// threads.
class G915HidppControlSession final : public application::KeyboardControlSession {
public:
    G915HidppControlSession(platform::hidpp::HidppChannel& channel, std::uint8_t deviceIndex)
        : channel_(&channel), deviceIndex_(deviceIndex)
    {
    }

    [[nodiscard]] shared_kernel::Result<void> setGameMode(const domain::GameModeKeys& keys) override;
    [[nodiscard]] shared_kernel::Result<domain::OnboardSlots> readSlots() override;
    [[nodiscard]] shared_kernel::Result<void> selectSlot(domain::OnboardSlot slot) override;

private:
    [[nodiscard]] shared_kernel::Result<platform::hidpp::HidppChannel::Response>
    call(std::uint16_t featureId, std::uint8_t function, std::span<const std::uint8_t> params = {});

    platform::hidpp::HidppChannel* channel_; // não nulo
    std::uint8_t deviceIndex_;
};

} // namespace ghub::lighting::infrastructure
