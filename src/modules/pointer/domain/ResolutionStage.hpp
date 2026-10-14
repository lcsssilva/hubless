#pragma once

#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace ghub::pointer {

// Dados brutos de um estágio de resolução (DPI), ainda não validados.
struct ResolutionStageSpec {
    std::uint32_t index;
    std::uint32_t dpiX;
    std::optional<std::uint32_t> dpiY; // ausente: DPI único para os dois eixos
    bool active;
    bool isDefault;
    bool disabled;
    std::vector<std::uint32_t> supportedDpis; // positivos, sem repetição, crescentes

    friend bool operator==(const ResolutionStageSpec&, const ResolutionStageSpec&) = default;
};

// Estágio de DPI como o dispositivo o relata. Objeto de valor imutável; só existe válido.
// O DPI atual pode não constar na lista de suportados (o ratbagd gera a lista a partir de min..max).
class ResolutionStage {
public:
    [[nodiscard]] static shared_kernel::Result<ResolutionStage> create(ResolutionStageSpec spec);

    [[nodiscard]] std::uint32_t index() const noexcept { return spec_.index; }
    [[nodiscard]] std::uint32_t dpiX() const noexcept { return spec_.dpiX; }
    [[nodiscard]] std::optional<std::uint32_t> dpiY() const noexcept { return spec_.dpiY; }
    [[nodiscard]] bool isActive() const noexcept { return spec_.active; }
    [[nodiscard]] bool isDefault() const noexcept { return spec_.isDefault; }
    [[nodiscard]] bool isDisabled() const noexcept { return spec_.disabled; }
    [[nodiscard]] const std::vector<std::uint32_t>& supportedDpis() const noexcept { return spec_.supportedDpis; }

    // O DPI X (e o Y, quando presente) consta na lista de suportados. Lista vazia: falso.
    [[nodiscard]] bool isDpiListed() const;

    friend bool operator==(const ResolutionStage&, const ResolutionStage&) = default;

private:
    explicit ResolutionStage(ResolutionStageSpec spec) : spec_(std::move(spec)) {}

    ResolutionStageSpec spec_;
};

} // namespace ghub::pointer
