/*---------------------------------------------------------*\
| RGBController_LogitechG915.cpp                            |
|                                                           |
|   RGBController for Logitech G915                         |
|                                                           |
|   Cheerpipe                                   20 Mar 2021 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

// Nota (G HUB para Linux): este arquivo é DERIVADO do OpenRGB (GPL-2.0-or-later).
//  - Origem: Controllers/LogitechController/LogitechG915Controller/RGBController_LogitechG915.cpp,
//    tag release_1.0 do OpenRGB (https://gitlab.com/CalcProgrammer1/OpenRGB), linhas 61-181 (led_names[]).
//  - Extraído: a tabela led_names[] (nome, zona, índice), 117 entradas, na mesma ordem. Os nomes das
//    macros KEY_EN_* foram resolvidos para o texto de RGBController/RGBControllerKeyNames.cpp
//    (mesma tag); "Logo" e "Key: G1".."Key: G5" são literais do original. As constantes
//    LOGITECH_G915_ZONE_* foram trocadas por seus valores numéricos (LogitechG915Controller.h:22-27
//    e :39-42): KEYBOARD=0x01, MEDIA=0x02, GKEYS=0x03, MODIFIERS=0x04, LOGO=0x10, INDICATORS=0x40.
//  - Mudou: tipos (KeyDefinition/KeyId), busca por nome e por endereço, e a consulta TKL; nenhum
//    código do OpenRGB além desta tabela foi copiado. Nenhum protocolo aqui: só dados.

#include "modules/lighting/domain/G915KeyCatalog.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace ghub::lighting::domain {

namespace {

struct RawKey {
    std::string_view name;
    std::uint8_t zone;
    std::uint8_t index;
};

// NOLINTBEGIN(modernize-use-designated-initializers, readability-trailing-comma) -- tabela de dados: {nome, zona,
// índice}
constexpr std::array<RawKey, G915KeyCatalog::kFullLayoutKeyCount> kRawKeys{{
    {"Key: A", 0x01, 0x04},
    {"Key: B", 0x01, 0x05},
    {"Key: C", 0x01, 0x06},
    {"Key: D", 0x01, 0x07},
    {"Key: E", 0x01, 0x08},
    {"Key: F", 0x01, 0x09},
    {"Key: G", 0x01, 0x0A},
    {"Key: H", 0x01, 0x0B},
    {"Key: I", 0x01, 0x0C},
    {"Key: J", 0x01, 0x0D},
    {"Key: K", 0x01, 0x0E},
    {"Key: L", 0x01, 0x0F},
    {"Key: M", 0x01, 0x10},
    {"Key: N", 0x01, 0x11},
    {"Key: O", 0x01, 0x12},
    {"Key: P", 0x01, 0x13},
    {"Key: Q", 0x01, 0x14},
    {"Key: R", 0x01, 0x15},
    {"Key: S", 0x01, 0x16},
    {"Key: T", 0x01, 0x17},
    {"Key: U", 0x01, 0x18},
    {"Key: V", 0x01, 0x19},
    {"Key: W", 0x01, 0x1A},
    {"Key: X", 0x01, 0x1B},
    {"Key: Y", 0x01, 0x1C},
    {"Key: Z", 0x01, 0x1D},
    {"Key: 1", 0x01, 0x1E},
    {"Key: 2", 0x01, 0x1F},
    {"Key: 3", 0x01, 0x20},
    {"Key: 4", 0x01, 0x21},
    {"Key: 5", 0x01, 0x22},
    {"Key: 6", 0x01, 0x23},
    {"Key: 7", 0x01, 0x24},
    {"Key: 8", 0x01, 0x25},
    {"Key: 9", 0x01, 0x26},
    {"Key: 0", 0x01, 0x27},
    {"Key: Enter", 0x01, 0x28},
    {"Key: Escape", 0x01, 0x29},
    {"Key: Backspace", 0x01, 0x2A},
    {"Key: Tab", 0x01, 0x2B},
    {"Key: Space", 0x01, 0x2C},
    {"Key: -", 0x01, 0x2D},
    {"Key: =", 0x01, 0x2E},
    {"Key: [", 0x01, 0x2F},
    {"Key: ]", 0x01, 0x30},
    {"Key: \\ (ANSI)", 0x01, 0x31},
    {"Key: #", 0x01, 0x32},
    {"Key: ;", 0x01, 0x33},
    {"Key: '", 0x01, 0x34},
    {"Key: `", 0x01, 0x35},
    {"Key: ,", 0x01, 0x36},
    {"Key: .", 0x01, 0x37},
    {"Key: /", 0x01, 0x38},
    {"Key: Caps Lock", 0x01, 0x39},
    {"Key: F1", 0x01, 0x3A},
    {"Key: F2", 0x01, 0x3B},
    {"Key: F3", 0x01, 0x3C},
    {"Key: F4", 0x01, 0x3D},
    {"Key: F5", 0x01, 0x3E},
    {"Key: F6", 0x01, 0x3F},
    {"Key: F7", 0x01, 0x40},
    {"Key: F8", 0x01, 0x41},
    {"Key: F9", 0x01, 0x42},
    {"Key: F10", 0x01, 0x43},
    {"Key: F11", 0x01, 0x44},
    {"Key: F12", 0x01, 0x45},
    {"Key: Print Screen", 0x01, 0x46},
    {"Key: Scroll Lock", 0x01, 0x47},
    {"Key: Pause/Break", 0x01, 0x48},
    {"Key: Insert", 0x01, 0x49},
    {"Key: Home", 0x01, 0x4A},
    {"Key: Page Up", 0x01, 0x4B},
    {"Key: Delete", 0x01, 0x4C},
    {"Key: End", 0x01, 0x4D},
    {"Key: Page Down", 0x01, 0x4E},
    {"Key: Right Arrow", 0x01, 0x4F},
    {"Key: Left Arrow", 0x01, 0x50},
    {"Key: Down Arrow", 0x01, 0x51},
    {"Key: Up Arrow", 0x01, 0x52},
    {"Key: \\ (ISO)", 0x01, 0x64},
    {"Key: Menu", 0x01, 0x65},
    {"Key: Left Control", 0x04, 0xE0},
    {"Key: Left Shift", 0x04, 0xE1},
    {"Key: Left Alt", 0x04, 0xE2},
    {"Key: Left Windows", 0x04, 0xE3},
    {"Key: Right Control", 0x04, 0xE4},
    {"Key: Right Shift", 0x04, 0xE5},
    {"Key: Right Alt", 0x04, 0xE6},
    {"Key: Right Windows", 0x04, 0xE7},
    {"Key: Media Previous", 0x02, 0x9E},
    {"Key: Media Play/Pause", 0x02, 0x9B},
    {"Key: Media Next", 0x02, 0x9D},
    {"Key: Media Mute", 0x02, 0x9C},
    {"Logo", 0x10, 0x01},
    {"Key: Brightness", 0x40, 0x99},
    {"Key: Num Lock", 0x01, 0x53},
    {"Key: Number Pad /", 0x01, 0x54},
    {"Key: Number Pad *", 0x01, 0x55},
    {"Key: Number Pad -", 0x01, 0x56},
    {"Key: Number Pad +", 0x01, 0x57},
    {"Key: Number Pad Enter", 0x01, 0x58},
    {"Key: Number Pad 1", 0x01, 0x59},
    {"Key: Number Pad 2", 0x01, 0x5A},
    {"Key: Number Pad 3", 0x01, 0x5B},
    {"Key: Number Pad 4", 0x01, 0x5C},
    {"Key: Number Pad 5", 0x01, 0x5D},
    {"Key: Number Pad 6", 0x01, 0x5E},
    {"Key: Number Pad 7", 0x01, 0x5F},
    {"Key: Number Pad 8", 0x01, 0x60},
    {"Key: Number Pad 9", 0x01, 0x61},
    {"Key: Number Pad 0", 0x01, 0x62},
    {"Key: Number Pad .", 0x01, 0x63},
    {"Key: G1", 0x03, 0x01},
    {"Key: G2", 0x03, 0x02},
    {"Key: G3", 0x03, 0x03},
    {"Key: G4", 0x03, 0x04},
    {"Key: G5", 0x03, 0x05},
}};
// NOLINTEND(modernize-use-designated-initializers, readability-trailing-comma)

} // namespace

std::span<const KeyDefinition> G915KeyCatalog::keys() noexcept
{
    static const auto catalog = [] {
        std::vector<KeyDefinition> result;
        result.reserve(kRawKeys.size());
        std::size_t position = 0; // NOLINT(misc-const-correctness) -- incrementado no laço
        for (const auto& raw : kRawKeys) {
            result.push_back(
                KeyDefinition{.id = KeyId{position++}, .name = raw.name, .zone = raw.zone, .index = raw.index});
        }
        return result;
    }();
    return catalog;
}

const KeyDefinition& G915KeyCatalog::definition(KeyId id) noexcept
{
    return keys()[id.position()]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- KeyId só
                                  // nasce do catálogo
}

std::optional<KeyId> G915KeyCatalog::findByName(std::string_view name) noexcept
{
    const auto all = keys();
    const auto found = std::ranges::find(all, name, &KeyDefinition::name);
    if (found == all.end()) {
        return std::nullopt;
    }
    return found->id;
}

std::optional<KeyId> G915KeyCatalog::findByAddress(std::uint8_t zone, std::uint8_t index) noexcept
{
    const auto all = keys();
    const auto found =
        std::ranges::find_if(all, [&](const KeyDefinition& key) { return key.zone == zone && key.index == index; });
    if (found == all.end()) {
        return std::nullopt;
    }
    return found->id;
}

} // namespace ghub::lighting::domain
