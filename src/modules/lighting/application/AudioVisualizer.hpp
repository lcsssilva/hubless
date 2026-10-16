#pragma once

#include "modules/audio_processing/api/AudioLevelSource.hpp"
#include "modules/lighting/application/RunSoftwareAnimation.hpp"
#include "modules/lighting/domain/RgbColor.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::lighting::application {

// INC-021 R3 (ADR-0030): visualizador de áudio do teclado. Liga a captura do `audio_processing` (consumida só pela
// api/ do módulo) apenas entre start() e stop()/destruição, e fornece a fonte de quadros que desenha as 10 bandas como
// barras no teclado (audioBarsFrame). Construir não abre nenhuma captura. A fonte de áudio precisa sobreviver a este
// objeto e à fonte de quadros devolvida.
class AudioVisualizer {
public:
    AudioVisualizer(audio_processing::AudioLevelSource& audio, domain::RgbColor color) noexcept;
    ~AudioVisualizer();
    AudioVisualizer(const AudioVisualizer&) = delete;
    AudioVisualizer& operator=(const AudioVisualizer&) = delete;
    AudioVisualizer(AudioVisualizer&&) = delete;
    AudioVisualizer& operator=(AudioVisualizer&&) = delete;

    [[nodiscard]] shared_kernel::Result<void> start();
    void stop();

    // Quadro = barras dos níveis atuais; ignora o tempo. Depois de stop() os níveis são zero (teclado apagado).
    [[nodiscard]] RunSoftwareAnimation::FrameSource frameSource() const;

private:
    audio_processing::AudioLevelSource* audio_; // não nulo
    domain::RgbColor color_;
};

} // namespace ghub::lighting::application
