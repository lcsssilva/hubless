#pragma once

#include <optional>
#include <string>

#include "modules/audio_processing/domain/EqualizerCurve.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::audio_processing {

// Recebe a perda do sink do EQ (G733 removido, nó destruído pelo PipeWire/WirePlumber, conexão perdida).
// Pode ser chamado em qualquer thread; quem implementa repassa para a thread de orquestração.
class AudioGraphObserver {
public:
    virtual ~AudioGraphObserver() = default;
    virtual void equalizerLost(const std::string& reason) = 0;

protected:
    AudioGraphObserver() = default;
    AudioGraphObserver(const AudioGraphObserver&) = default;
    AudioGraphObserver& operator=(const AudioGraphObserver&) = default;
    AudioGraphObserver(AudioGraphObserver&&) = default;
    AudioGraphObserver& operator=(AudioGraphObserver&&) = default;
};

// Port: grafo de áudio do host (ADR-0016). Operações bloqueantes com prazo de 5 s; erro de prazo ou de
// conexão -> BackendUnavailable. Só escreve o sink do EQ e a preferência de saída padrão (R9).
class AudioGraph {
public:
    virtual ~AudioGraph() = default;

    // node.name do sink de áudio do G733 (Audio/Device 046d:0afe). Ausente -> BackendUnavailable.
    [[nodiscard]] virtual shared_kernel::Result<std::string> findHeadsetSink() = 0;

    // Cria o sink do EQ com destino fixo em targetSink (sem fallback). Já existente -> InvalidArgument.
    [[nodiscard]] virtual shared_kernel::Result<void> createEqualizer(const EqualizerCurve& curve,
                                                                      const std::string& targetSink) = 0;
    // Só altera os controles do sink existente. Sem EQ -> InvalidArgument.
    [[nodiscard]] virtual shared_kernel::Result<void> updateEqualizer(const EqualizerCurve& curve) = 0;
    // Remove o sink do EQ; sem EQ é sucesso.
    [[nodiscard]] virtual shared_kernel::Result<void> removeEqualizer() = 0;

    // node.name do sink do EQ, como aparece na preferência de saída padrão.
    [[nodiscard]] virtual std::string equalizerSinkName() const = 0;

    // Preferência de saída padrão (metadata "default", default.configured.audio.sink); nullopt = ausente.
    [[nodiscard]] virtual shared_kernel::Result<std::optional<std::string>> configuredDefaultSink() = 0;
    // Grava a preferência; nullopt apaga a chave.
    [[nodiscard]] virtual shared_kernel::Result<void>
    setConfiguredDefaultSink(const std::optional<std::string>& sinkName) = 0;

    // Observador da perda; nullptr desliga. Deve durar mais que o EQ criado.
    virtual void setObserver(AudioGraphObserver* observer) = 0;

protected:
    AudioGraph() = default;
    AudioGraph(const AudioGraph&) = default;
    AudioGraph& operator=(const AudioGraph&) = default;
    AudioGraph(AudioGraph&&) = default;
    AudioGraph& operator=(AudioGraph&&) = default;
};

} // namespace ghub::audio_processing
