#include "modules/lighting/infrastructure/LightingHidppPolicy.hpp"

#include <array>

namespace ghub::lighting::infrastructure {

namespace {

constexpr std::uint16_t bit(unsigned function)
{
    return static_cast<std::uint16_t>(1U << function);
}

constexpr std::uint16_t kBrightnessFeature = 0x8040;
constexpr std::uint16_t kReportRateFeature = 0x8060;

struct Rule {
    std::uint16_t featureId;
    std::uint16_t functions; // máscara: bit N = função N permitida
};

// Lista fechada (ADR-0013 decisão 4 / INC-008 R3). Tudo que não consta aqui é recusado.
constexpr std::array<Rule, 8> allowedRules{{
    {0x0000, static_cast<std::uint16_t>(bit(0) | bit(1))},                             // Root
    {0x0001, static_cast<std::uint16_t>(bit(0) | bit(1))},                             // FeatureSet
    {0x8071, static_cast<std::uint16_t>(bit(0) | bit(1) | bit(5))},                    // RGB effects
    {0x8081, static_cast<std::uint16_t>(bit(0) | bit(1) | bit(6) | bit(7))},           // per-key lighting
    {0x8070, static_cast<std::uint16_t>(bit(0) | bit(1) | bit(2) | bit(3) | bit(14))}, // color LED effects
    // Onboard profiles, SOMENTE leitura: fn0 GetProfilesDescr, fn2 GetOnboardMode, fn4 GetCurrentProfile,
    // fn5 MemoryRead (libratbag src/hidpp20.c:1783-1793@8235b5bb). Escritas (fn1 SetOnboardMode,
    // fn3 SetCurrentProfile, fn6-8 Memory*Write, fn12) ficam recusadas.
    {0x8100, static_cast<std::uint16_t>(bit(0) | bit(2) | bit(4) | bit(5))},
    // Leituras de estado (INC-015 parte 2, ADR-0025), SEM parâmetros: 0x8040 fn0/fn1 (brilho máximo e atual) e
    // 0x8060 fn0/fn1 (períodos suportados e atual). fn2+ (escritas, como a SetReportRate 0x8060 fn2) recusadas.
    {kBrightnessFeature, static_cast<std::uint16_t>(bit(0) | bit(1))},
    {kReportRateFeature, static_cast<std::uint16_t>(bit(0) | bit(1))},
}};

// Lista explícita de bloqueio: nunca enviada. Semântica desconhecida (ADR-0013, decisão do usuário de 2026-10-14).
constexpr std::array<std::uint16_t, 1> blockedFeatures{0x4522};

} // namespace

bool LightingHidppPolicy::isAllowed(std::uint16_t featureId, std::uint8_t function) noexcept
{
    if (function > 15 || isExplicitlyBlocked(featureId)) {
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
    return isAllowed(featureId, function);
}

bool LightingHidppPolicy::isExplicitlyBlocked(std::uint16_t featureId) noexcept
{
    for (const auto blocked : blockedFeatures) {
        if (blocked == featureId) {
            return true;
        }
    }
    return false;
}

} // namespace ghub::lighting::infrastructure
