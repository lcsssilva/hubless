#pragma once

#include "modules/audio_processing/api/AudioLevelSource.hpp"

#include <memory>
#include <string>

namespace ghub::audio_processing {

// Adaptador de AudioLevelSource sobre libpipewire (INC-021 R3, ADR-0030): um fluxo de captura (`pw_stream`) do monitor
// da saída padrão (`stream.capture.sink`; quem liga o fluxo é o gerenciador de sessão), mono F32 a 48 kHz (o PipeWire
// converte), analisado por BandMeter. Nada é aberto na construção: a conexão, o loop e o fluxo só existem entre start()
// e stop()/destrutor, e as amostras nunca saem do adaptador (só os 10 níveis). remoteName vazio = PipeWire da sessão;
// um caminho de socket seleciona outro daemon (testes). Só LÊ o grafo: não cria links nem altera metadata.
class PipeWireAudioLevelSource final : public AudioLevelSource {
public:
    explicit PipeWireAudioLevelSource(std::string remoteName = {});
    ~PipeWireAudioLevelSource() override;
    PipeWireAudioLevelSource(const PipeWireAudioLevelSource&) = delete;
    PipeWireAudioLevelSource& operator=(const PipeWireAudioLevelSource&) = delete;
    PipeWireAudioLevelSource(PipeWireAudioLevelSource&&) = delete;
    PipeWireAudioLevelSource& operator=(PipeWireAudioLevelSource&&) = delete;

    [[nodiscard]] shared_kernel::Result<void> start() override;
    void stop() override;
    [[nodiscard]] bool running() const override;
    [[nodiscard]] std::array<float, 10> levels() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ghub::audio_processing
