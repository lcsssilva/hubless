#pragma once

#include "modules/lighting/domain/GameModeKeys.hpp"
#include "modules/lighting/domain/OnboardSlot.hpp"
#include "shared_kernel/Result.hpp"

#include <memory>

namespace ghub::lighting::application {

// Sessão aberta com o G915 para os controles que não são iluminação ao vivo (INC-019, ADR-0029): modo de jogo e
// troca de slot onboard. Quem a abre (ControlKeyboard) já segura o DeviceLease. Não é segura para uso concorrente.
class KeyboardControlSession {
public:
    virtual ~KeyboardControlSession() = default;

    // Modo de jogo: fn3 (limpa) e depois fn1 com os códigos (vazio = todas as teclas reativadas).
    [[nodiscard]] virtual shared_kernel::Result<void> setGameMode(const domain::GameModeKeys& keys) = 0;
    // Só leitura: slots habilitados no diretório da memória onboard e o slot ativo.
    [[nodiscard]] virtual shared_kernel::Result<domain::OnboardSlots> readSlots() = 0;
    // 0x8100 fn3 `00 <n>`. Não confere se o slot está habilitado: quem chama (ControlKeyboard) já conferiu.
    [[nodiscard]] virtual shared_kernel::Result<void> selectSlot(domain::OnboardSlot slot) = 0;

protected:
    KeyboardControlSession() = default;
    KeyboardControlSession(const KeyboardControlSession&) = default;
    KeyboardControlSession& operator=(const KeyboardControlSession&) = default;
    KeyboardControlSession(KeyboardControlSession&&) = default;
    KeyboardControlSession& operator=(KeyboardControlSession&&) = default;
};

// Port: abre uma sessão de controle do teclado. Abrir não envia nada.
// Erro BackendUnavailable = teclado ausente ou nó inacessível.
class KeyboardControl {
public:
    virtual ~KeyboardControl() = default;
    [[nodiscard]] virtual shared_kernel::Result<std::unique_ptr<KeyboardControlSession>> open() = 0;

protected:
    KeyboardControl() = default;
    KeyboardControl(const KeyboardControl&) = default;
    KeyboardControl& operator=(const KeyboardControl&) = default;
    KeyboardControl(KeyboardControl&&) = default;
    KeyboardControl& operator=(KeyboardControl&&) = default;
};

} // namespace ghub::lighting::application
