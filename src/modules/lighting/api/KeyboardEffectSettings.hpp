#pragma once

#include "shared_kernel/Result.hpp"

#include <mutex>
#include <optional>
#include <string>

namespace ghub::lighting {

// Efeito de prévia do G915 como o módulo `profiles` o guarda. kind: wave, cycle, breathing, ripple ou static; cor
// 0-255 por canal (vale para breathing, ripple e static); brilho 1-100 % (vale para wave, cycle e breathing).
struct KeyboardEffectValue {
    std::string kind;
    int red;
    int green;
    int blue;
    int brightness;

    friend bool operator==(const KeyboardEffectValue&, const KeyboardEffectValue&) = default;
};

// Port da sessão ao vivo do teclado (a que a janela já usa, INC-008 parte C / R14): quem a possui liga o modo ao
// vivo se preciso e envia o efeito com os mesmos cuidados da prévia (sondagem, DeviceLease, uma tentativa). Chamado
// só na thread dona da sessão. Sucesso = pedido aceito pela sessão; o resultado do envio aparece no estado dela.
class KeyboardEffectApplier {
public:
    virtual ~KeyboardEffectApplier() = default;
    [[nodiscard]] virtual shared_kernel::Result<void> applyEffect(const KeyboardEffectValue& value) = 0;

protected:
    KeyboardEffectApplier() = default;
    KeyboardEffectApplier(const KeyboardEffectApplier&) = default;
    KeyboardEffectApplier& operator=(const KeyboardEffectApplier&) = default;
    KeyboardEffectApplier(KeyboardEffectApplier&&) = default;
    KeyboardEffectApplier& operator=(KeyboardEffectApplier&&) = default;
};

// API pública do módulo `lighting` para a seção de efeito do perfil (INC-017 parte 2, R10; ADR-0028). Valida o
// efeito pelas faixas do domínio e o entrega à sessão ao vivo; não abre sessão própria nem grava nada no teclado
// (o efeito é volátil). Guarda o "último efeito" porque o G915 não informa o efeito de prévia em vigor. O aplicador
// precisa sobreviver à fachada. Seguro entre threads quanto à memória do último efeito.
class KeyboardEffectSettings {
public:
    explicit KeyboardEffectSettings(KeyboardEffectApplier& applier) noexcept : applier_(&applier) {}

    KeyboardEffectSettings(const KeyboardEffectSettings&) = delete;
    KeyboardEffectSettings& operator=(const KeyboardEffectSettings&) = delete;
    KeyboardEffectSettings(KeyboardEffectSettings&&) = delete;
    KeyboardEffectSettings& operator=(KeyboardEffectSettings&&) = delete;
    ~KeyboardEffectSettings() = default;

    // Tipo desconhecido, cor fora de 0-255 ou brilho fora de 1-100 -> InvalidArgument sem repassar.
    [[nodiscard]] shared_kernel::Result<KeyboardEffectValue> apply(const KeyboardEffectValue& value);

    [[nodiscard]] std::optional<KeyboardEffectValue> last() const;
    // Registra o que a janela aplicou por fora da fachada; não toca o teclado.
    void remember(const KeyboardEffectValue& value);

private:
    KeyboardEffectApplier* applier_; // não nulo
    mutable std::mutex mutex_;
    std::optional<KeyboardEffectValue> last_;
};

} // namespace ghub::lighting
