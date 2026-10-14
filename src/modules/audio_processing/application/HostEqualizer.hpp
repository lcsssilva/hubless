#pragma once

#include <optional>
#include <string>

#include "modules/audio_processing/application/AudioGraph.hpp"
#include "modules/audio_processing/domain/EqualizerCurve.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::audio_processing {

// Caso de uso do EQ de saída no host (ADR-0016, R3-R6). Não é thread-safe: chamado só pela thread que
// serializa as operações (a de trabalho do controlador). Não escreve nada na construção.
class HostEqualizer {
public:
    explicit HostEqualizer(AudioGraph& graph) noexcept;

    // R3: encontra o G733, cria o EQ com a curva atual, guarda a preferência de saída padrão e grava a do EQ.
    // Falha ao gravar a preferência -> remove o EQ e devolve o erro. Já ligado -> sucesso sem nada fazer.
    [[nodiscard]] shared_kernel::Result<void> enable();

    // R4: ligado -> atualiza só os controles; desligado -> só guarda a curva. Falha na atualização mantém a
    // curva anterior.
    [[nodiscard]] shared_kernel::Result<void> changeCurve(const EqualizerCurve& curve);

    // R5: se a preferência ainda for o EQ, restaura a guardada (ou apaga a chave); depois remove o EQ.
    // Desligado -> sucesso sem nada fazer. Termina desligado mesmo com erro (o primeiro erro é devolvido).
    [[nodiscard]] shared_kernel::Result<void> disable();

    // R6: o EQ sumiu fora do nosso controle. Restaura a preferência (regra do R5) e fica desligado.
    void equalizerLost();

    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] const EqualizerCurve& curve() const noexcept { return curve_; }

private:
    AudioGraph& graph_;
    EqualizerCurve curve_ = EqualizerCurve::flat();
    bool enabled_ = false;
    std::optional<std::string> savedDefault_;
};

} // namespace ghub::audio_processing
