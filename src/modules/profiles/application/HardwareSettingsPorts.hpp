#pragma once

#include "modules/profiles/domain/HardwareSections.hpp"
#include "shared_kernel/Result.hpp"

#include <optional>
#include <vector>

namespace ghub::profiles {

// Portas para as seções de hardware do perfil (INC-017 parte 2, R10; ADR-0028). Cada adaptador (em infrastructure)
// chama somente a `api/` do módulo dono. O perfil nunca valida faixas: quem valida é o dono, ao aplicar.

// Sidetone e luzes do G733 (módulo headset). O headset não informa o valor atual: `last*` devolve o último valor
// aplicado pela janela (ou pelo próprio perfil) nesta sessão. Aplicar é bloqueante: thread de trabalho.
class HeadsetSettingsPort {
public:
    virtual ~HeadsetSettingsPort() = default;
    [[nodiscard]] virtual std::optional<int> lastSidetone() const = 0;
    [[nodiscard]] virtual std::vector<HeadsetLightSetting> lastLights() const = 0;
    // Devolve o nível aplicado.
    [[nodiscard]] virtual shared_kernel::Result<int> applySidetone(int level) = 0;
    [[nodiscard]] virtual shared_kernel::Result<void> applyLight(const HeadsetLightSetting& light) = 0;

protected:
    HeadsetSettingsPort() = default;
    HeadsetSettingsPort(const HeadsetSettingsPort&) = default;
    HeadsetSettingsPort& operator=(const HeadsetSettingsPort&) = default;
    HeadsetSettingsPort(HeadsetSettingsPort&&) = default;
    HeadsetSettingsPort& operator=(HeadsetSettingsPort&&) = default;
};

// Efeito de prévia do G915 (módulo lighting). Volátil. Só pode ser chamado na thread dona da sessão ao vivo
// (a mesma da janela): `apply` devolve "pedido aceito" e o resultado do envio aparece no estado da sessão.
class KeyboardEffectPort {
public:
    virtual ~KeyboardEffectPort() = default;
    [[nodiscard]] virtual std::optional<KeyboardEffectSetting> last() const = 0;
    [[nodiscard]] virtual shared_kernel::Result<void> apply(const KeyboardEffectSetting& effect) = 0;

protected:
    KeyboardEffectPort() = default;
    KeyboardEffectPort(const KeyboardEffectPort&) = default;
    KeyboardEffectPort& operator=(const KeyboardEffectPort&) = default;
    KeyboardEffectPort(KeyboardEffectPort&&) = default;
    KeyboardEffectPort& operator=(KeyboardEffectPort&&) = default;
};

struct PointerDpiApplied {
    PointerDpiSetting setting; // relido do mouse
    bool wrote;                // false = o mouse já tinha estes valores
};

// DPI/estágios/taxa do G703 (módulo pointer). `readCurrent` lê o perfil onboard ativo; `apply` grava nele pelo
// caminho seguro do dono (cópia, releitura, restauração). Bloqueantes: thread de trabalho.
class PointerSettingsPort {
public:
    virtual ~PointerSettingsPort() = default;
    [[nodiscard]] virtual shared_kernel::Result<PointerDpiSetting> readCurrent() = 0;
    [[nodiscard]] virtual shared_kernel::Result<PointerDpiApplied> apply(const PointerDpiSetting& setting) = 0;

protected:
    PointerSettingsPort() = default;
    PointerSettingsPort(const PointerSettingsPort&) = default;
    PointerSettingsPort& operator=(const PointerSettingsPort&) = default;
    PointerSettingsPort(PointerSettingsPort&&) = default;
    PointerSettingsPort& operator=(PointerSettingsPort&&) = default;
};

// Conjunto opcional das portas de hardware; nulo = módulo ausente (a seção falha com "indisponível").
struct HardwarePorts {
    HeadsetSettingsPort* headset = nullptr;
    KeyboardEffectPort* keyboard = nullptr;
    PointerSettingsPort* pointer = nullptr;
};

} // namespace ghub::profiles
