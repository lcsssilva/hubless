#pragma once

#include <array>
#include <cstddef>

#include "shared_kernel/Result.hpp"

namespace ghub::audio_processing {

// Curva do EQ gráfico de saída processado no host (ADR-0016): 10 bandas fixas, ganho inteiro em dB.
// Objeto de valor imutável; só existe válido.
class EqualizerCurve {
public:
    static constexpr std::size_t bandCount = 10;
    static constexpr std::array<int, bandCount> bandFrequenciesHz{32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    static constexpr int minimumGainDb = -12;
    static constexpr int maximumGainDb = 12;

    // Todas as bandas em 0 dB.
    [[nodiscard]] static EqualizerCurve flat() noexcept;

    // Algum ganho fora de -12..12 -> InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<EqualizerCurve> create(const std::array<int, bandCount>& gainsDb);

    // Nova curva com uma banda alterada. Banda >= 10 ou ganho fora da faixa -> InvalidArgument.
    [[nodiscard]] shared_kernel::Result<EqualizerCurve> withBand(std::size_t band, int gainDb) const;

    [[nodiscard]] const std::array<int, bandCount>& gainsDb() const noexcept { return gainsDb_; }

    // Pré-ganho que evita saturação: -(maior ganho positivo), ou 0 se nenhum for positivo (R2).
    [[nodiscard]] int preampDb() const noexcept;

    friend bool operator==(const EqualizerCurve&, const EqualizerCurve&) = default;

private:
    explicit EqualizerCurve(const std::array<int, bandCount>& gainsDb) noexcept : gainsDb_(gainsDb) {}

    std::array<int, bandCount> gainsDb_;
};

} // namespace ghub::audio_processing
