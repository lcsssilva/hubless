#pragma once

#include "modules/lighting/domain/KeyboardEffect.hpp"
#include "shared_kernel/Result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace ghub::lighting::domain {

// Entrada de iluminação do perfil onboard do G915 (INC-019 R4, ADR-0029): 11 bytes `[efeito][p0..p9]` no bloco 0xD0
// do setor, no mesmo formato do SetEffect da 0x8071. Só os formatos vistos nas cópias reais da memória (2026-10-15)
// são interpretados; qualquer outro é "bruto": nunca decodificado nem reescrito por este código.
//
//   Static    01 R G B 02 00..          (marcador 02 em p3)
//   Breathing 0a R G B PH PL 01 BR 00.. (período BE16 em p3/p4, p5 = 01, brilho em p6)
//   Cycle     03 00 00 00 00 00 PH PL BR (período BE16 em p5/p6, brilho em p7)
//   Wave      04 .. PL 01 BR PH         (período baixo em p6, direção 01 em p7, brilho em p8, período alto em p9)
//   Ripple    0b R G B 00 00 SP 00..    (velocidade em p5)
inline constexpr std::size_t kStoredEffectSize = 11;
using StoredEffectBytes = std::array<std::uint8_t, kStoredEffectSize>;

// nullopt = formato sem evidência (bruto) ou valores fora das faixas do domínio.
[[nodiscard]] std::optional<KeyboardEffect> decodeStoredEffect(const StoredEffectBytes& entry);

// Bytes da entrada para o efeito pedido. Se `original` já guarda um efeito do mesmo tipo, só os campos editáveis
// mudam e os demais bytes de `original` seguem iguais; se o tipo muda (ou `original` é bruto), a entrada é montada
// do molde do tipo. `Off` não tem formato gravado: InvalidArgument.
[[nodiscard]] shared_kernel::Result<StoredEffectBytes> encodeStoredEffect(const KeyboardEffect& wanted,
                                                                          const StoredEffectBytes& original);

// Entradas 2 e 3 do bloco acompanham as 0 e 1 em todas as cópias reais (mesmo efeito, com brilho 50 nos tipos que têm
// brilho). Dadas a primária antes/depois da edição e a variante atual, devolve a nova variante: igual se a primária
// não mudou; se o tipo é o mesmo, a variante recebe os campos editados e mantém o próprio brilho; se o tipo mudou, é
// o molde do novo tipo com brilho 50. A função das entradas 2 e 3 no firmware não está confirmada.
[[nodiscard]] StoredEffectBytes deriveDimmedVariant(const StoredEffectBytes& oldPrimary,
                                                    const StoredEffectBytes& oldVariant,
                                                    const StoredEffectBytes& newPrimary);

} // namespace ghub::lighting::domain
