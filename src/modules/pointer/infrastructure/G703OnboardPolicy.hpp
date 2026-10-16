#pragma once

#include "modules/onboard_memory/api/HidppOnboardMemory.hpp"
#include "platform/hidpp/HidppCommandPolicy.hpp"

#include <cstdint>
#include <span>

namespace ghub::pointer::infrastructure {

// Lista fechada de comandos HID++ 2.0 que o módulo pointer pode enviar ao G703 (ADR-0017 §5, INC-011 R8):
//   - Root fn0/fn1 e FeatureSet fn0/fn1;
//   - 0x8100 fn0, fn2, fn4, fn5 (leituras);
//   - 0x8100 fn6 (endereço de escrita) só com os 6 parâmetros [setor BE16][0000][tamanho BE16] e setor entre 1 e
//     `profileCount` (nunca o diretório 0x0000 nem a ROM >= 0x0100); fn7 e fn8 (corpo e fim da escrita);
//   - leituras de estado (INC-015 parte 2, ADR-0025): 0x2201 fn1/fn2 só com params exatamente [00] (sensor 0) e
//     0x8060 fn0/fn1 sem parâmetros; 0x2201 fn0/fn3+ e 0x8060 fn2+ (escritas) continuam recusados;
//   - nada mais: fn1 (modo), fn3 (perfil atual), fn9..15 e qualquer outra feature são recusados.
// `profileCount` só é conhecido após o fn0: começa em 0 (fn6 sempre recusado) e só o adaptador HidppOnboardMemory
// o fixa (OnboardProfilePolicy::setProfileCount), depois de um describe() bem-sucedido.
class G703OnboardPolicy final : public platform::hidpp::HidppCommandPolicy,
                                public onboard_memory::OnboardProfilePolicy {
public:
    explicit G703OnboardPolicy(std::uint8_t profileCount = 0) noexcept : profileCount_(profileCount) {}

    void setProfileCount(std::uint8_t profileCount) noexcept override { profileCount_ = profileCount; }

    [[nodiscard]] bool isAllowed(std::uint16_t featureId, std::uint8_t function,
                                 std::span<const std::uint8_t> params) const noexcept override;

private:
    std::uint8_t profileCount_;
};

} // namespace ghub::pointer::infrastructure
