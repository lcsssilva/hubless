#pragma once

#include "modules/onboard_memory/api/HidppOnboardMemory.hpp"
#include "platform/hidpp/HidppCommandPolicy.hpp"

#include <cstdint>
#include <span>

namespace ghub::lighting::infrastructure {

// Lista fechada de comandos HID++ 2.0 que a memória onboard do G915 pode enviar (INC-019 R5, ADR-0029, ADR-0017 §5):
//   - Root fn0/fn1 e FeatureSet fn0/fn1 (descoberta do índice da 0x8100);
//   - 0x8100 fn0, fn2, fn4, fn5 (leituras);
//   - 0x8100 fn6 só com os 6 parâmetros `[setor BE16][0000][00ff]` (captura C-06, 2026-10-15) e setor entre 1 e
//     min(`profileCount`, 3) (nunca o diretório 0x0000 nem a ROM >= 0x0100); fn7 com exatamente 16 bytes e fn8 sem
//     parâmetros (corpo e fim da escrita);
//   - nada mais: fn1 (modo), fn3 (troca de slot, do ControlKeyboard), fn9..15 e qualquer outra feature.
// `profileCount` só é conhecido após o fn0: começa em 0 (fn6 sempre recusado) e só o adaptador HidppOnboardMemory o fixa
// (OnboardProfilePolicy::setProfileCount), depois de um describe() bem-sucedido. Que o setor seja o do perfil ativo é
// garantido pelo ActiveSectorEditor, não por esta política.
class G915OnboardPolicy final : public platform::hidpp::HidppCommandPolicy,
                                public onboard_memory::OnboardProfilePolicy {
public:
    explicit G915OnboardPolicy(std::uint8_t profileCount = 0) noexcept : profileCount_(profileCount) {}

    void setProfileCount(std::uint8_t profileCount) noexcept override { profileCount_ = profileCount; }

    [[nodiscard]] bool isAllowed(std::uint16_t featureId, std::uint8_t function,
                                 std::span<const std::uint8_t> params) const noexcept override;

private:
    std::uint8_t profileCount_;
};

} // namespace ghub::lighting::infrastructure
