#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "modules/audio_processing/application/AudioGraph.hpp"
#include "modules/audio_processing/application/NoiseSuppressionGraph.hpp"

namespace ghub::audio_processing {

// Adaptador dos ports AudioGraph (EQ da saída, ADR-0016) e NoiseSuppressionGraph (redução de ruído do microfone,
// ADR-0018) sobre libpipewire, numa só conexão (um pw_thread_loop, um registro). Cada filtro é uma instância
// independente: perda e observador próprios. Thread-safe: todas as chamadas passam pelo lock do
// pw_thread_loop. Nada conecta na construção; a conexão é feita na primeira operação e desfeita no destrutor.
// remoteName vazio = PipeWire da sessão; um caminho absoluto de socket seleciona outro daemon (testes).
class PipeWireAudioGraph final : public AudioGraph, public NoiseSuppressionGraph {
public:
    explicit PipeWireAudioGraph(std::string remoteName = {},
                                std::chrono::milliseconds deadline = std::chrono::seconds{5});
    ~PipeWireAudioGraph() override;
    PipeWireAudioGraph(const PipeWireAudioGraph&) = delete;
    PipeWireAudioGraph& operator=(const PipeWireAudioGraph&) = delete;
    PipeWireAudioGraph(PipeWireAudioGraph&&) = delete;
    PipeWireAudioGraph& operator=(PipeWireAudioGraph&&) = delete;

    [[nodiscard]] shared_kernel::Result<std::string> findHeadsetSink() override;
    [[nodiscard]] shared_kernel::Result<void> createEqualizer(const EqualizerCurve& curve,
                                                              const std::string& targetSink) override;
    [[nodiscard]] shared_kernel::Result<void> updateEqualizer(const EqualizerCurve& curve) override;
    // Antes de destruir o sink, espera até 2 s que nenhum Link do registro tenha o sink do EQ como nó de entrada
    // (o WirePlumber move os fluxos ao mudar a saída padrão; destruir antes faz apps como o Spotify pausarem). Passado
    // o prazo destrói assim mesmo (fluxos fixados no EQ pelo usuário nunca saem). Sem conexão ou sem EQ: sem espera. O
    // destrutor aplica a mesma regra.
    [[nodiscard]] shared_kernel::Result<void> removeEqualizer() override;
    [[nodiscard]] std::string equalizerSinkName() const override;
    [[nodiscard]] shared_kernel::Result<std::optional<std::string>> configuredDefaultSink() override;
    [[nodiscard]] shared_kernel::Result<void>
    setConfiguredDefaultSink(const std::optional<std::string>& sinkName) override;
    void setObserver(AudioGraphObserver* observer) override;

    [[nodiscard]] shared_kernel::Result<std::string> findHeadsetSource() override;
    // Plugin RNNoise ausente (LADSPA_PATH ou caminhos padrão) -> BackendUnavailable citando o pacote; nada fica criado.
    [[nodiscard]] shared_kernel::Result<void> createNoiseSuppression(VoiceThreshold threshold,
                                                                     const std::string& source) override;
    [[nodiscard]] shared_kernel::Result<void> updateNoiseSuppression(VoiceThreshold threshold) override;
    // Antes de destruir a fonte filtrada, espera até 2 s que nenhum Link saia dela (mesma ideia do EQ, no sentido
    // oposto: aplicativos que gravam são movidos pelo WirePlumber). Passado o prazo destrói assim mesmo.
    [[nodiscard]] shared_kernel::Result<void> removeNoiseSuppression() override;
    [[nodiscard]] std::string noiseSuppressionSourceName() const override;
    [[nodiscard]] shared_kernel::Result<std::optional<std::string>> configuredDefaultSource() override;
    [[nodiscard]] shared_kernel::Result<void>
    setConfiguredDefaultSource(const std::optional<std::string>& sourceName) override;
    void setNoiseSuppressionObserver(NoiseSuppressionObserver* observer) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ghub::audio_processing
