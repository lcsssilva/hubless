/*---------------------------------------------------------*\
| LogitechG915Controller.cpp                                |
|                                                           |
|   Driver for Logitech G915                                |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

// Nota (G HUB para Linux): este arquivo é DERIVADO do OpenRGB (GPL-2.0-or-later).
//  - Origem: Controllers/LogitechController/LogitechG915Controller/LogitechG915Controller.cpp, tag release_1.0
//    (https://gitlab.com/CalcProgrammer1/OpenRGB), SendMode (:384-431): posição dos bytes de cor, período,
//    brilho, direção e velocidade em cada modo (usb_buf[0x06] = p0). IDs de efeito iguais aos do GetInfo.
//  - Mudou: função pura que devolve ID + p0..p9 (sem hidapi, sem buffer de 20 bytes, sem escrita); o período já
//    chega em milissegundos (o OpenRGB envia 100 * velocidade, que é o mesmo valor).

#include "modules/lighting/infrastructure/G915EffectFrames.hpp"

namespace ghub::lighting::infrastructure {

namespace {

using domain::KeyboardEffectKind;

constexpr std::uint8_t high(std::uint32_t value) noexcept
{
    return static_cast<std::uint8_t>(value / 256U % 256U);
}
constexpr std::uint8_t low(std::uint32_t value) noexcept
{
    return static_cast<std::uint8_t>(value % 256U);
}

constexpr std::uint8_t staticMarker = 0x02;
constexpr std::uint8_t waveHorizontal = 0x01;

void setColor(G915EffectEncoding& encoded, const domain::RgbColor& color) noexcept
{
    encoded.params[0] = color.red();
    encoded.params[1] = color.green();
    encoded.params[2] = color.blue();
}

} // namespace

G915EffectEncoding encodeG915Effect(const domain::KeyboardEffect& effect)
{
    G915EffectEncoding encoded{.effectId = 0x0000, .params = {}};
    // As fábricas de KeyboardEffect garantem que os parâmetros de cada tipo existem; value_or(0) só evita acesso
    // desprotegido ao optional.
    const auto periodMs = effect.period().transform([](auto p) { return p.milliseconds(); }).value_or(0);
    const auto percent = effect.brightness().transform([](auto b) { return b.percent(); }).value_or(0);
    switch (effect.kind()) {
    case KeyboardEffectKind::Off:
        break;
    case KeyboardEffectKind::Static:
        encoded.effectId = 0x0001;
        setColor(encoded, effect.color().value_or(domain::RgbColor{0, 0, 0}));
        encoded.params[3] = staticMarker;
        break;
    case KeyboardEffectKind::Breathing:
        encoded.effectId = 0x000A;
        setColor(encoded, effect.color().value_or(domain::RgbColor{0, 0, 0}));
        encoded.params[3] = high(periodMs);
        encoded.params[4] = low(periodMs);
        encoded.params[6] = static_cast<std::uint8_t>(percent);
        break;
    case KeyboardEffectKind::Cycle:
        encoded.effectId = 0x0003;
        encoded.params[5] = high(periodMs);
        encoded.params[6] = low(periodMs);
        encoded.params[7] = static_cast<std::uint8_t>(percent);
        break;
    case KeyboardEffectKind::Wave:
        encoded.effectId = 0x0004;
        encoded.params[6] = low(periodMs);
        encoded.params[7] = waveHorizontal;
        encoded.params[8] = static_cast<std::uint8_t>(percent);
        encoded.params[9] = high(periodMs);
        break;
    case KeyboardEffectKind::Ripple:
        encoded.effectId = 0x000B;
        setColor(encoded, effect.color().value_or(domain::RgbColor{0, 0, 0}));
        encoded.params[5] = low(effect.rippleSpeed().transform([](auto s) { return s.value(); }).value_or(0));
        break;
    }
    return encoded;
}

} // namespace ghub::lighting::infrastructure
