#include "modules/lighting/infrastructure/LightingHidppPolicy.hpp"

#include "modules/lighting/domain/GameModeKeys.hpp"
#include "modules/lighting/domain/OnboardSlot.hpp"

#include <algorithm>
#include <array>

namespace ghub::lighting::infrastructure {

namespace {

constexpr std::uint16_t bit(unsigned function)
{
    return static_cast<std::uint16_t>(1U << function);
}

constexpr std::uint16_t kBrightnessFeature = 0x8040;
constexpr std::uint16_t kReportRateFeature = 0x8060;
constexpr std::uint16_t kOnboardProfilesFeature = 0x8100;
// Modo de jogo (INC-019 R2, ADR-0029): fn1 = lista de códigos HID a desativar, fn3 = limpa. Captura C-04, 2026-10-15.
constexpr std::uint16_t kGameModeFeature = 0x4522;
constexpr unsigned kGameModeSetFunction = 1;
constexpr unsigned kGameModeClearFunction = 3;
constexpr unsigned kSelectProfileFunction = 3;

struct Rule {
    std::uint16_t featureId;
    std::uint16_t functions; // máscara: bit N = função N permitida
};

// Lista fechada (ADR-0013 decisão 4 / INC-008 R3). Tudo que não consta aqui é recusado.
constexpr std::array<Rule, 9> allowedRules{{
    {0x0000, static_cast<std::uint16_t>(bit(0) | bit(1))},                             // Root
    {0x0001, static_cast<std::uint16_t>(bit(0) | bit(1))},                             // FeatureSet
    {0x8071, static_cast<std::uint16_t>(bit(0) | bit(1) | bit(5))},                    // RGB effects
    {0x8081, static_cast<std::uint16_t>(bit(0) | bit(1) | bit(6) | bit(7))},           // per-key lighting
    {0x8070, static_cast<std::uint16_t>(bit(0) | bit(1) | bit(2) | bit(3) | bit(14))}, // color LED effects
    // Onboard profiles: leituras fn0 GetProfilesDescr, fn2 GetOnboardMode, fn4 GetCurrentProfile, fn5 MemoryRead
    // (libratbag src/hidpp20.c:1783-1793@8235b5bb) e, desde o INC-019 (ADR-0029), fn3 SetCurrentProfile só com
    // `00 <1..3>` (troca de slot, captura C-04). fn1 SetOnboardMode, fn6-8 Memory*Write e fn12 ficam recusadas.
    {kOnboardProfilesFeature, static_cast<std::uint16_t>(bit(0) | bit(2) | bit(3) | bit(4) | bit(5))},
    // Modo de jogo: fn1 (SetKeys, 0..16 códigos do catálogo) e fn3 (limpa, sem parâmetros). fn0/fn2+ recusadas.
    {kGameModeFeature, static_cast<std::uint16_t>(bit(kGameModeSetFunction) | bit(kGameModeClearFunction))},
    // Leituras de estado (INC-015 parte 2, ADR-0025), SEM parâmetros: 0x8040 fn0/fn1 (brilho máximo e atual) e
    // 0x8060 fn0/fn1 (períodos suportados e atual). fn2+ (escritas, como a SetReportRate 0x8060 fn2) recusadas.
    {kBrightnessFeature, static_cast<std::uint16_t>(bit(0) | bit(1))},
    {kReportRateFeature, static_cast<std::uint16_t>(bit(0) | bit(1))},
}};

} // namespace

bool LightingHidppPolicy::isAllowed(std::uint16_t featureId, std::uint8_t function) noexcept
{
    if (function > 15) {
        return false;
    }
    for (const auto& rule : allowedRules) {
        if (rule.featureId == featureId) {
            return (rule.functions & bit(function)) != 0;
        }
    }
    return false;
}

bool LightingHidppPolicy::isAllowed(std::uint16_t featureId, std::uint8_t function,
                                    std::span<const std::uint8_t> params) const noexcept
{
    // As leituras de estado não levam parâmetros: qualquer conteúdo é recusado (auditoria de protocolo).
    if ((featureId == kBrightnessFeature || featureId == kReportRateFeature) && !params.empty()) {
        return false;
    }
    if (!isAllowed(featureId, function)) {
        return false;
    }
    if (featureId == kGameModeFeature) {
        if (function == kGameModeClearFunction) {
            return params.empty();
        }
        return params.size() <= domain::GameModeKeys::kMaxKeys && std::ranges::all_of(params, [](std::uint8_t code) {
                   return domain::GameModeKeys::isEligibleCode(code);
               });
    }
    if (featureId == kOnboardProfilesFeature && function == kSelectProfileFunction) {
        // SetCurrentProfile [setor BE16]: só os setores 1..3 do G915.
        return params.size() == 2 && params[0] == 0 && domain::OnboardSlot::create(params[1]).has_value();
    }
    return true;
}

} // namespace ghub::lighting::infrastructure
