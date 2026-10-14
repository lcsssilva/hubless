#pragma once

#include "shared_kernel/Result.hpp"

namespace ghub::audio_processing {

// Sensibilidade da redução de ruído do microfone no host (ADR-0018): limiar de detecção de voz do RNNoise, 0-99 %.
// Abaixo do limiar o som é cortado. Objeto de valor imutável; só existe válido.
class VoiceThreshold {
public:
    static constexpr int minimumPercent = 0;
    static constexpr int maximumPercent = 99;

    // 50 %, o valor do exemplo do PipeWire (source-rnnoise.conf).
    [[nodiscard]] static VoiceThreshold standard() noexcept;

    // Fora de 0-99 -> InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<VoiceThreshold> create(int percent);

    [[nodiscard]] int percent() const noexcept { return percent_; }

    friend bool operator==(const VoiceThreshold&, const VoiceThreshold&) = default;

private:
    explicit VoiceThreshold(int percent) noexcept : percent_(percent) {}

    int percent_;
};

} // namespace ghub::audio_processing
