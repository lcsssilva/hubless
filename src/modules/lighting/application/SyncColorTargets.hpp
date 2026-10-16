#pragma once

#include "modules/headset/api/HeadsetProfileSettings.hpp"
#include "modules/lighting/api/KeyboardEffectSettings.hpp"
#include "modules/lighting/application/SyncColor.hpp"
#include "modules/pointer/api/G703LedSettings.hpp"

#include <functional>

namespace ghub::lighting::application {

// Alvos da sincronização de cor (INC-021 R5) sobre as api/ públicas dos módulos donos (ADR-0002).

// G915: efeito "static" pela prévia ao vivo (volátil). `isLive` diz se o modo ao vivo está ligado (sem ele, o
// teclado é pulado). Chamado só na thread dona da sessão ao vivo.
class KeyboardEffectTarget final : public KeyboardColorTarget {
public:
    KeyboardEffectTarget(KeyboardEffectSettings& settings, std::function<bool()> isLive)
        : settings_(&settings), isLive_(std::move(isLive))
    {
    }
    [[nodiscard]] bool ready() const override { return isLive_ && isLive_(); }
    [[nodiscard]] shared_kernel::Result<void> applyFixed(const domain::RgbColor& color) override;

private:
    KeyboardEffectSettings* settings_; // não nulo
    std::function<bool()> isLive_;
};

// G733: efeito fixo (`01 R G B 02`) nas duas zonas (volátil). Tenta as duas zonas mesmo se uma falhar; devolve o
// primeiro erro.
class HeadsetFixedTarget final : public HeadsetColorTarget {
public:
    explicit HeadsetFixedTarget(const headset::HeadsetProfileSettings& settings) noexcept : settings_(&settings) {}
    [[nodiscard]] shared_kernel::Result<void> applyFixed(const domain::RgbColor& color) override;

private:
    const headset::HeadsetProfileSettings* settings_; // não nulo
};

// G703: LED primário e logotipo fixos no perfil onboard ativo (persistente; cópia/releitura do caminho existente).
class MouseLedTarget final : public MouseColorTarget {
public:
    explicit MouseLedTarget(const pointer::G703LedSettings& settings) noexcept : settings_(&settings) {}
    [[nodiscard]] shared_kernel::Result<void> applyFixed(const domain::RgbColor& color) override;

private:
    const pointer::G703LedSettings* settings_; // não nulo
};

} // namespace ghub::lighting::application
