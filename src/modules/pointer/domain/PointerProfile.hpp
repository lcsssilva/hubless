#pragma once

#include "modules/pointer/domain/ButtonMapping.hpp"
#include "modules/pointer/domain/LedState.hpp"
#include "modules/pointer/domain/ResolutionStage.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ghub::pointer {

// Dados brutos de um perfil, ainda não validados.
struct PointerProfileSpec {
    std::uint32_t index;
    std::string name; // pode ser vazio (o G703 não nomeia perfis)
    bool active;
    bool enabled;
    std::uint32_t reportRate;                        // Hz; 0 = desconhecido
    std::vector<std::uint32_t> supportedReportRates; // positivas, sem repetição
    std::vector<ResolutionStage> stages;             // índices únicos
    std::vector<ButtonMapping> buttons;              // índices únicos
    std::vector<LedState> leds;                      // índices únicos

    friend bool operator==(const PointerProfileSpec&, const PointerProfileSpec&) = default;
};

// Perfil do mouse como o dispositivo o relata. `active` e `enabled` são independentes: o G703 real
// relata o perfil ativo como desabilitado (evidence/2026-10-13-ratbagd-leitura.md). Objeto de valor
// imutável; só existe válido.
class PointerProfile {
public:
    [[nodiscard]] static shared_kernel::Result<PointerProfile> create(PointerProfileSpec spec);

    [[nodiscard]] std::uint32_t index() const noexcept { return spec_.index; }
    [[nodiscard]] const std::string& name() const noexcept { return spec_.name; }
    [[nodiscard]] bool isActive() const noexcept { return spec_.active; }
    [[nodiscard]] bool isEnabled() const noexcept { return spec_.enabled; }
    [[nodiscard]] std::uint32_t reportRate() const noexcept { return spec_.reportRate; }
    [[nodiscard]] const std::vector<std::uint32_t>& supportedReportRates() const noexcept
    {
        return spec_.supportedReportRates;
    }
    [[nodiscard]] const std::vector<ResolutionStage>& stages() const noexcept { return spec_.stages; }
    [[nodiscard]] const std::vector<ButtonMapping>& buttons() const noexcept { return spec_.buttons; }
    [[nodiscard]] const std::vector<LedState>& leds() const noexcept { return spec_.leds; }

    friend bool operator==(const PointerProfile&, const PointerProfile&) = default;

private:
    explicit PointerProfile(PointerProfileSpec spec) : spec_(std::move(spec)) {}

    PointerProfileSpec spec_;
};

} // namespace ghub::pointer
