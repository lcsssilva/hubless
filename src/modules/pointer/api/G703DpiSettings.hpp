#pragma once

#include "modules/pointer/application/G703OnboardSettings.hpp"
#include "shared_kernel/Result.hpp"

#include <optional>
#include <vector>

namespace ghub::pointer {

// Taxa de relatório, estágios de DPI e estágios padrão/shift do G703 como o módulo `profiles` os guarda (valores
// simples; a validação é do domínio do pointer).
struct G703DpiValue {
    int reportRateHz;
    std::vector<int> dpiStages;
    int defaultStage;
    // nullopt ao aplicar = manter o estágio do shift que o mouse já tem (tirar um shift válido é recusado, ADR-0027).
    std::optional<int> dpiShiftStage;

    friend bool operator==(const G703DpiValue&, const G703DpiValue&) = default;
};

struct G703DpiApplied {
    G703DpiValue value; // relido do mouse depois da gravação
    bool wrote;         // false = o mouse já tinha estes valores; nada foi enviado
};

// API pública do módulo `pointer` para a seção de DPI do perfil (INC-017 parte 2, R10; ADR-0028). Fachada fina sobre
// G703OnboardSettings: toda gravação passa pelo caminho seguro do onboard_memory (sonda, DeviceLease, cópia
// automática, releitura e comparação; ADR-0017). Os LEDs do perfil onboard nunca são alterados por aqui. Bloqueante:
// thread de trabalho. O caso de uso precisa sobreviver à fachada.
class G703DpiSettings {
public:
    explicit G703DpiSettings(const G703OnboardSettings& onboard) noexcept : onboard_(&onboard) {}

    // Só leitura do setor ativo.
    [[nodiscard]] shared_kernel::Result<G703DpiValue> readCurrent() const;

    // Valores fora das faixas do domínio (taxa 125/250/500/1000 Hz, 1-5 estágios de 100-25600 DPI em múltiplos de
    // 50, índices existentes) -> InvalidArgument sem escrever.
    [[nodiscard]] shared_kernel::Result<G703DpiApplied> apply(const G703DpiValue& wanted) const;

private:
    const G703OnboardSettings* onboard_; // não nulo
};

} // namespace ghub::pointer
