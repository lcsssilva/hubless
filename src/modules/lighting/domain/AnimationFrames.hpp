#pragma once

#include "modules/lighting/domain/G915KeyCatalog.hpp"
#include "modules/lighting/domain/KeyColorMap.hpp"
#include "modules/lighting/domain/RgbColor.hpp"
#include "shared_kernel/Result.hpp"

#include <array>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

namespace ghub::lighting::domain {

// INC-021 R1 (ADR-0030): funções de quadro puras `quadro(t) -> KeyColorMap`. Sem relógio, sem I/O, sem estado: o mesmo
// tempo e os mesmos parâmetros dão sempre o mesmo quadro. O tempo `timeMs` é medido desde o início da animação.
// Todo quadro cobre TODAS as teclas do catálogo (as apagadas em preto), para que o envio só das teclas que mudaram
// (RunSoftwareAnimation) também apague o que saiu de cena.
// As teclas não têm geometria no catálogo: a posição horizontal é aproximada pela posição da tecla no catálogo
// (ordem de led_names[] do OpenRGB, que segue o teclado da esquerda para a direita por fileiras).

// Ciclo de cor: todas as teclas na mesma cor, percorrendo a roda de matiz em `periodMs` (mínimo efetivo 1 ms).
struct ColorCycleParams {
    std::uint32_t periodMs;
};

enum class WaveDirection : std::uint8_t { LeftToRight, RightToLeft };

// Onda: a roda de matiz espalhada pelo catálogo, deslocando-se na direção escolhida; uma volta em `periodMs`.
struct WaveParams {
    std::uint32_t periodMs;
    WaveDirection direction;
};

// Estrela: `densityPercent` (0..100, acima disso vale 100) das teclas piscam com `color` em triângulo de 2000 ms; a
// escolha das teclas e as fases (em passos de 100 ms) saem da `seed` (injetada, para teste determinístico).
struct StarParams {
    std::uint8_t densityPercent;
    RgbColor color;
    std::uint32_t seed;
};

struct Keyframe {
    std::uint32_t atMs;
    RgbColor color;
};

// Cores por tempo com interpolação linear; repete a cada `atMs` do último keyframe. Antes do primeiro, vale a cor dele.
class KeyframeTrack {
public:
    // Vazio ou tempos não estritamente crescentes -> InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<KeyframeTrack> create(std::vector<Keyframe> frames);

    [[nodiscard]] RgbColor colorAt(std::int64_t timeMs) const;

private:
    explicit KeyframeTrack(std::vector<Keyframe> frames) : frames_(std::move(frames)) {}
    std::vector<Keyframe> frames_;
};

[[nodiscard]] KeyColorMap colorCycleFrame(std::int64_t timeMs, const ColorCycleParams& params);
[[nodiscard]] KeyColorMap waveFrame(std::int64_t timeMs, const WaveParams& params);
[[nodiscard]] KeyColorMap starFrame(std::int64_t timeMs, const StarParams& params);
[[nodiscard]] KeyColorMap keyframeFrame(std::int64_t timeMs, const KeyframeTrack& track);

// Animação escolhida pelo usuário: um dos quatro geradores com seus parâmetros.
using AnimationSpec = std::variant<ColorCycleParams, WaveParams, StarParams, KeyframeTrack>;
[[nodiscard]] KeyColorMap animationFrame(std::int64_t timeMs, const AnimationSpec& spec);

// Visualizador (R3): 10 bandas dividem o catálogo em 10 faixas de posições; em cada uma acendem-se os primeiros
// ceil(nível * n) teclas com `color`, o resto em preto. Nível fora de 0..1 é limitado; NaN vale 0.
[[nodiscard]] KeyColorMap audioBarsFrame(const std::array<float, 10>& levels, RgbColor color);

} // namespace ghub::lighting::domain
