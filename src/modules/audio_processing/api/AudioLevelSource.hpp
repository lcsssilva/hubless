#pragma once

#include "shared_kernel/Result.hpp"

#include <array>

namespace ghub::audio_processing {

// API pública do módulo `audio_processing` (INC-021 R3, ADR-0030): níveis por banda da saída de áudio padrão, para o
// visualizador do `lighting`. Privacidade: a captura (monitor da saída padrão) só existe entre start() e stop(); fora
// disso nenhum fluxo de áudio é aberto. Só os 10 níveis (0..1) saem do módulo; as amostras nunca são gravadas nem
// registradas em log. As 10 bandas são as do EQ (32 Hz..16 kHz).
// Thread-safe: levels() pode ser chamado de qualquer thread enquanto o áudio chega em outra.
class AudioLevelSource {
public:
    virtual ~AudioLevelSource() = default;

    // Abre a captura. Já ligada -> sucesso sem efeito. Sem PipeWire -> BackendUnavailable (e nada fica aberto).
    [[nodiscard]] virtual shared_kernel::Result<void> start() = 0;
    // Fecha a captura e zera os níveis. Idempotente.
    virtual void stop() = 0;
    [[nodiscard]] virtual bool running() const = 0;
    // Níveis atuais; tudo zero quando parada ou sem som.
    [[nodiscard]] virtual std::array<float, 10> levels() const = 0;

protected:
    AudioLevelSource() = default;
    AudioLevelSource(const AudioLevelSource&) = default;
    AudioLevelSource& operator=(const AudioLevelSource&) = default;
    AudioLevelSource(AudioLevelSource&&) = default;
    AudioLevelSource& operator=(AudioLevelSource&&) = default;
};

} // namespace ghub::audio_processing
