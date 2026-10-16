#pragma once

#include "modules/lighting/domain/KeyColorMap.hpp"
#include "modules/lighting/domain/RgbColor.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace ghub::lighting::domain {

// INC-021 R4 (ADR-0030): redução de um quadro de tela a cores médias por região. Funções puras, sem I/O: o quadro
// entra como visão somente leitura e nada dele é guardado; só as médias saem.

// Ordem dos bytes de cada pixel de 4 bytes (o quarto byte é X ou alfa e é sempre ignorado).
enum class PixelFormat : std::uint8_t {
    Bgrx, // B, G, R, X (também BGRA)
    Rgbx, // R, G, B, X (também RGBA)
};

struct ScreenFrameView {
    std::span<const std::uint8_t> pixels;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t stride; // bytes entre o começo de duas linhas (>= width * 4)
    PixelFormat format;
};

struct RegionGrid {
    std::uint32_t columns;
    std::uint32_t rows;
};

// Média por canal de cada região da grade, em ordem de linhas (linha 0 primeiro, da esquerda para a direita), com
// arredondamento ao mais próximo. Região = intervalo [c*largura/colunas, (c+1)*largura/colunas) (idem nas linhas),
// então cobre todo pixel uma única vez. `sampleStep` (>= 1; 0 vale 1) lê um pixel a cada `sampleStep` em x e em y
// dentro da região, para reduzir o custo em telas grandes. InvalidArgument: dimensões ou grade nulas, mais
// colunas/linhas que pixels, stride menor que a linha, ou buffer menor que o quadro (a última linha não precisa de
// preenchimento).
[[nodiscard]] shared_kernel::Result<std::vector<RgbColor>> averageRegions(const ScreenFrameView& frame, RegionGrid grid,
                                                                          std::uint32_t sampleStep = 1);

// Quadro do teclado: as 10 regiões dividem o catálogo em 10 faixas de posições (a mesma divisão do visualizador de
// áudio) e cada tecla recebe a cor da sua faixa. Número de regiões diferente de 10 -> teclado todo preto.
[[nodiscard]] KeyColorMap screenRegionsFrame(std::span<const RgbColor> regions);

} // namespace ghub::lighting::domain
