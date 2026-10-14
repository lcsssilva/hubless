#pragma once

#include "shared_kernel/Result.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace ghub::profiles {

// Curva do EQ gráfico: 10 ganhos em dB. A faixa (-12..12) não é validada aqui: quem valida é o módulo
// audio_processing ao aplicar, para a regra não ficar duplicada (ADR-0020).
class EqualizerGains {
public:
    static constexpr std::size_t bandCount = 10;

    // Tamanho diferente de 10 -> InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<EqualizerGains> create(const std::vector<int>& gains);

    [[nodiscard]] const std::array<int, bandCount>& values() const noexcept { return values_; }

    friend bool operator==(const EqualizerGains&, const EqualizerGains&) = default;

private:
    explicit EqualizerGains(const std::array<int, bandCount>& values) noexcept : values_(values) {}

    std::array<int, bandCount> values_;
};

} // namespace ghub::profiles
