#pragma once

#include <optional>
#include <string>

#include "modules/audio_processing/domain/VoiceThreshold.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::audio_processing {

// Recebe a perda da fonte filtrada (microfone do G733 removido, nó destruído, conexão perdida). Qualquer thread.
class NoiseSuppressionObserver {
public:
    virtual ~NoiseSuppressionObserver() = default;
    virtual void noiseSuppressionLost(const std::string& reason) = 0;

protected:
    NoiseSuppressionObserver() = default;
    NoiseSuppressionObserver(const NoiseSuppressionObserver&) = default;
    NoiseSuppressionObserver& operator=(const NoiseSuppressionObserver&) = default;
    NoiseSuppressionObserver(NoiseSuppressionObserver&&) = default;
    NoiseSuppressionObserver& operator=(NoiseSuppressionObserver&&) = default;
};

// Port: redução de ruído do microfone no host (ADR-0018). Mesmas regras de prazo e erro do AudioGraph (5 s,
// BackendUnavailable). Só escreve a fonte filtrada e a preferência de entrada padrão.
class NoiseSuppressionGraph {
public:
    virtual ~NoiseSuppressionGraph() = default;

    // node.name do Audio/Source do G733 (Audio/Device 046d:0afe). Ausente -> BackendUnavailable.
    [[nodiscard]] virtual shared_kernel::Result<std::string> findHeadsetSource() = 0;

    // Cria a fonte filtrada lendo de source (sem fallback). Plugin ausente -> BackendUnavailable com o nome do
    // pacote; já existente -> InvalidArgument.
    [[nodiscard]] virtual shared_kernel::Result<void> createNoiseSuppression(VoiceThreshold threshold,
                                                                             const std::string& source) = 0;
    // Só altera o limiar da fonte existente. Sem fonte -> InvalidArgument.
    [[nodiscard]] virtual shared_kernel::Result<void> updateNoiseSuppression(VoiceThreshold threshold) = 0;
    // Remove a fonte filtrada (esperando os fluxos saírem, até 2 s); sem fonte é sucesso.
    [[nodiscard]] virtual shared_kernel::Result<void> removeNoiseSuppression() = 0;

    // node.name da fonte filtrada, como aparece na preferência de entrada padrão.
    [[nodiscard]] virtual std::string noiseSuppressionSourceName() const = 0;

    // Preferência de entrada padrão (metadata "default", default.configured.audio.source); nullopt = ausente.
    [[nodiscard]] virtual shared_kernel::Result<std::optional<std::string>> configuredDefaultSource() = 0;
    // Grava a preferência; nullopt apaga a chave.
    [[nodiscard]] virtual shared_kernel::Result<void>
    setConfiguredDefaultSource(const std::optional<std::string>& sourceName) = 0;

    // Observador da perda; nullptr desliga. Depois que retorna, o observador antigo não é mais chamado.
    virtual void setNoiseSuppressionObserver(NoiseSuppressionObserver* observer) = 0;

protected:
    NoiseSuppressionGraph() = default;
    NoiseSuppressionGraph(const NoiseSuppressionGraph&) = default;
    NoiseSuppressionGraph& operator=(const NoiseSuppressionGraph&) = default;
    NoiseSuppressionGraph(NoiseSuppressionGraph&&) = default;
    NoiseSuppressionGraph& operator=(NoiseSuppressionGraph&&) = default;
};

} // namespace ghub::audio_processing
