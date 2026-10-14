#pragma once

#include "modules/lighting/domain/KeyColorMap.hpp"
#include "modules/lighting/domain/KeyboardEffect.hpp"
#include "modules/lighting/domain/KeyboardZone.hpp"
#include "shared_kernel/Result.hpp"

#include <memory>

namespace ghub::lighting::application {

// Sessão aberta com o teclado (INC-008 partes C e D, ADR-0013). O modo ao vivo não grava nada; setEffect envia um
// efeito de hardware. Não é segura para uso concorrente; quem a abre a usa de uma thread só. Destruir a sessão fecha o
// nó (RAII), mas não devolve o controle ao firmware: isso é responsabilidade de quem chamou beginLive
// (LiveKeyboardSession).
class KeyboardLightingSession {
public:
    virtual ~KeyboardLightingSession() = default;

    // Toma o controle da iluminação (efeito volátil, sob o host).
    [[nodiscard]] virtual shared_kernel::Result<void> beginLive() = 0;
    // Pinta as teclas do mapa; as demais ficam como estão. Mapa vazio não envia nada.
    [[nodiscard]] virtual shared_kernel::Result<void> paint(const domain::KeyColorMap& colors) = 0;
    // Envia o efeito de hardware da zona (0x8071 SetEffect, byte de persistência 01 como OpenRGB/Solaar; com 00 o
    // firmware aceita sem aplicar, OpenRGB RGBController_LogitechHIDPP20.cpp:3180-3192@81bbe18a). Exige beginLive.
    // Nesta unidade o efeito só aparece sob controle do host: depois de endLive o efeito do firmware volta (testes
    // D1/D2, 2026-10-14). Efeito ausente na zona -> InvalidArgument sem escrever. Não reenvia.
    [[nodiscard]] virtual shared_kernel::Result<void> setEffect(domain::KeyboardZone zone,
                                                                const domain::KeyboardEffect& effect) = 0;
    // Devolve o controle da iluminação ao firmware.
    [[nodiscard]] virtual shared_kernel::Result<void> endLive() = 0;
    // true quando o firmware retomou o controle (repouso/reconexão do teclado, INC-008 R15). Só lê, nunca escreve.
    [[nodiscard]] virtual shared_kernel::Result<bool> controlLost() = 0;

protected:
    KeyboardLightingSession() = default;
    KeyboardLightingSession(const KeyboardLightingSession&) = default;
    KeyboardLightingSession& operator=(const KeyboardLightingSession&) = default;
    KeyboardLightingSession(KeyboardLightingSession&&) = default;
    KeyboardLightingSession& operator=(KeyboardLightingSession&&) = default;
};

// Port: abre uma sessão com o teclado. Abrir não envia nada ao dispositivo.
// Erro BackendUnavailable = teclado ausente ou nó inacessível.
class KeyboardLighting {
public:
    virtual ~KeyboardLighting() = default;
    [[nodiscard]] virtual shared_kernel::Result<std::unique_ptr<KeyboardLightingSession>> open() = 0;

protected:
    KeyboardLighting() = default;
    KeyboardLighting(const KeyboardLighting&) = default;
    KeyboardLighting& operator=(const KeyboardLighting&) = default;
    KeyboardLighting(KeyboardLighting&&) = default;
    KeyboardLighting& operator=(KeyboardLighting&&) = default;
};

} // namespace ghub::lighting::application
