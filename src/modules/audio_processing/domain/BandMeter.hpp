#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace ghub::audio_processing {

// Medidor de níveis por banda (INC-021 R3, ADR-0030): guarda as últimas `kWindow` amostras mono e, em levels(), calcula
// 10 níveis (0..1) com FFT própria, janela de Hann e as mesmas 10 frequências centrais do EQ (32 Hz..16 kHz); os
// limites entre bandas são as médias geométricas dos centros (a primeira vai de 0 e a última até Nyquist). O nível de
// cada banda é o maior pico de amplitude (1,0 = senoide de escala cheia) na banda, convertido para dB e mapeado de -60
// dB..0 dB para 0..1. Silêncio dá exatamente 0. Amostra não finita conta como silêncio. Sem estado compartilhado: não é
// seguro para uso concorrente (o adaptador protege com mutex).
class BandMeter {
public:
    static constexpr std::size_t kWindow = 4096;
    static constexpr std::size_t kBands = 10;

    explicit BandMeter(double sampleRateHz);

    void push(std::span<const float> monoSamples);
    void clear();
    [[nodiscard]] std::array<float, kBands> levels() const;

private:
    double sampleRate_;
    std::vector<float> ring_; // kWindow amostras; a mais antiga em `next_`
    std::size_t next_ = 0;
    std::size_t filled_ = 0;
};

} // namespace ghub::audio_processing
