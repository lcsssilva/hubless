#pragma once

#include <string>
#include <vector>

#include "modules/audio_processing/domain/EqualizerCurve.hpp"
#include "modules/audio_processing/domain/VoiceThreshold.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::audio_processing {

// Controle de execução do filter-chain: "<nó>:<porta>" e valor (R4).
struct FilterChainControl {
    std::string name;
    float value;
};

// Texto SPA-JSON dos argumentos de libpipewire-module-filter-chain (ADR-0016, R7). Função pura.
// targetSink vazio ou com aspas, barra invertida ou caractere de controle -> InvalidArgument.
// remoteName (opcional, mesma validação) vira `remote.name`: o módulo, carregado num contexto cliente, conecta sozinho
// e sem isso usaria o PipeWire da sessão.
[[nodiscard]] shared_kernel::Result<std::string>
buildFilterChainArgs(const EqualizerCurve& curve, const std::string& targetSink, const std::string& remoteName = {});

// Pares de controle para atualizar a curva em execução: "pre:Gain 1" e "eq_1:Gain" ... "eq_10:Gain".
[[nodiscard]] std::vector<FilterChainControl> buildControlUpdates(const EqualizerCurve& curve);

// Argumentos do filter-chain da redução de ruído do microfone (ADR-0018, R7): um nó ladspa `rnnoise`
// (librnnoise_ladspa, noise_suppressor_mono) a 48 kHz mono; entradas/saídas inferidas do único nó. source (node.name do
// microfone do G733) e remoteName com a mesma validação do EQ.
[[nodiscard]] shared_kernel::Result<std::string>
buildNoiseSuppressionArgs(VoiceThreshold threshold, const std::string& source, const std::string& remoteName = {});

// Par de controle da sensibilidade em execução: "rnnoise:VAD Threshold (%)".
[[nodiscard]] std::vector<FilterChainControl> buildNoiseSuppressionControlUpdates(VoiceThreshold threshold);

} // namespace ghub::audio_processing
