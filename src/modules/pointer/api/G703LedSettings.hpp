#pragma once

#include "modules/pointer/application/G703OnboardSettings.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::pointer {

// Cor 0-255 por canal (valores simples; a validação é daqui).
struct G703LedColor {
    int red;
    int green;
    int blue;
};

struct G703LedApplied {
    bool wrote; // false = o mouse já tinha estes LEDs; nada foi enviado
};

// API pública do módulo `pointer` para a sincronização de cor (INC-021 R5; ADR-0030). Fachada fina sobre
// G703OnboardSettings: grava o LED primário e o logotipo como "fixo" no perfil onboard ativo, mantendo taxa, estágios
// de DPI e shift como o mouse os tem. Toda gravação passa pelo caminho seguro do onboard_memory (sonda, DeviceLease,
// cópia automática, releitura e comparação; ADR-0017). É persistente. Bloqueante: thread de trabalho. O caso de uso
// precisa sobreviver à fachada.
class G703LedSettings {
public:
    explicit G703LedSettings(const G703OnboardSettings& onboard) noexcept : onboard_(&onboard) {}

    // Canal fora de 0-255 -> InvalidArgument sem ler nem escrever.
    [[nodiscard]] shared_kernel::Result<G703LedApplied> applyFixed(const G703LedColor& color) const;

private:
    const G703OnboardSettings* onboard_; // não nulo
};

} // namespace ghub::pointer
