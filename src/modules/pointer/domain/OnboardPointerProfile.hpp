#pragma once

#include "modules/pointer/domain/PointerConfiguration.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <span>

namespace ghub::pointer {

// Perfil ativo do G703 lido do setor onboard, no modelo de leitura do módulo (INC-016 B2-B5, ADR-0027): um único
// perfil, numerado pelo número do setor (a partir de 1, como na 0x8100 fn4), ativo e habilitado; estágios de DPI
// (o padrão fica como ativo e padrão), taxa, botões (offset 0x20) e os dois LEDs (offset 0xD0). Os bytes têm CRC
// já validado por quem os leu. Formatos de botão e de LED sem evidência própria ficam como bytes brutos.
[[nodiscard]] shared_kernel::Result<PointerConfiguration>
pointerConfigurationFromOnboardSector(std::uint16_t sectorNumber, std::span<const std::uint8_t> sector);

} // namespace ghub::pointer
