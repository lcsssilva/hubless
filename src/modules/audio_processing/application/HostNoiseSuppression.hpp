#pragma once

#include <optional>
#include <string>

#include "modules/audio_processing/application/NoiseSuppressionGraph.hpp"
#include "modules/audio_processing/domain/VoiceThreshold.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::audio_processing {

// Caso de uso da redução de ruído do microfone no host (ADR-0018, R2-R6). Mesma máquina de estados do
// HostEqualizer, sobre a entrada padrão. Não é thread-safe; não escreve nada na construção.
class HostNoiseSuppression {
public:
    explicit HostNoiseSuppression(NoiseSuppressionGraph& graph) noexcept;

    // Encontra o microfone do G733, cria a fonte filtrada com o limiar atual, guarda a preferência de entrada
    // padrão e grava a da fonte filtrada. Falha ao ler/gravar a preferência -> remove a fonte e devolve o erro.
    // Já ligado -> sucesso sem nada fazer.
    [[nodiscard]] shared_kernel::Result<void> enable();

    // Ligado -> só atualiza o limiar; desligado -> só guarda. Falha mantém o limiar anterior.
    [[nodiscard]] shared_kernel::Result<void> changeThreshold(VoiceThreshold threshold);

    // Restaura a preferência guardada se ela ainda for a fonte filtrada; depois remove. Termina desligado mesmo
    // com erro (devolve o primeiro).
    [[nodiscard]] shared_kernel::Result<void> disable();

    // A fonte sumiu fora do nosso controle: mesma regra do disable, resultado descartado.
    void noiseSuppressionLost();

    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] VoiceThreshold threshold() const noexcept { return threshold_; }

private:
    NoiseSuppressionGraph& graph_;
    VoiceThreshold threshold_ = VoiceThreshold::standard();
    bool enabled_ = false;
    std::optional<std::string> savedDefault_;
};

} // namespace ghub::audio_processing
