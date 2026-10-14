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
//  - Origem: Controllers/LogitechController/LogitechG915Controller/RGBController_LogitechG915.cpp, tag release_1.0
//    (https://gitlab.com/CalcProgrammer1/OpenRGB): DATA_FRAME_SIZE/BIG_FRAME_MAX_KEYS/LITTLE_FRAME_MAX_KEYS
//    (:19-21), terminate_buffer/populate_little_frame_data/populate_big_frame_data (:207-265) e o agrupamento
//    por cor e a tradução de zona em DeviceUpdateLEDs (:449-583). Tipos de quadro de LogitechG915Controller.h:32-33.
//  - Mudou: entrada é um KeyColorMap (todas as teclas do mapa, sem o cache de "cores já enviadas"); a saída é uma
//    lista de quadros (função + parâmetros) em vez de chamadas ao controlador; os parâmetros terminam no byte 0xFF
//    (os zeros seguintes são preenchidos pelo canal, como o memset do original). Sem hidapi, sem escrita aqui.

#include "modules/lighting/infrastructure/G915PerKeyFrames.hpp"

#include "modules/lighting/domain/G915KeyCatalog.hpp"

#include <cstddef>
#include <map>

namespace ghub::lighting::infrastructure {

namespace {

constexpr std::size_t dataFrameSize = 16;
constexpr std::size_t bigFrameMaxKeys = 13;
constexpr std::size_t littleFrameMaxKeys = 4;
constexpr std::uint8_t terminator = 0xFF;

// Funções da 0x8081 usadas pelo OpenRGB (tipos 0x6F e 0x1F com o software ID dele).
constexpr std::uint8_t bigFrameFunction = 6;
constexpr std::uint8_t littleFrameFunction = 1;

// Zonas de led_names[] (LogitechG915Controller.h:22-27, 39-42).
constexpr std::uint8_t zoneKeyboard = 0x01;
constexpr std::uint8_t zoneGKeys = 0x03;
constexpr std::uint8_t zoneModifiers = 0x04;
constexpr std::uint8_t zoneLogo = 0x10;

// Mesma chave de ordenação do std::map<RGBColor, …> do original: RGBColor = R | G << 8 | B << 16.
std::uint32_t openRgbOrder(domain::RgbColor color)
{
    return static_cast<std::uint32_t>(color.red()) | (static_cast<std::uint32_t>(color.green()) << 8U) |
           (static_cast<std::uint32_t>(color.blue()) << 16U);
}

struct LittleEntry {
    std::uint8_t key;
    domain::RgbColor color;
};

PerKeyFrame bigFrame(domain::RgbColor color, const std::vector<std::uint8_t>& keys, std::size_t first,
                     std::size_t count)
{
    PerKeyFrame frame{.function = bigFrameFunction, .params = {color.red(), color.green(), color.blue()}};
    for (std::size_t i = 0; i < count; ++i) {
        frame.params.push_back(keys[first + i]);
    }
    if (count < bigFrameMaxKeys) {
        frame.params.push_back(terminator);
    }
    return frame;
}

PerKeyFrame littleFrame(const std::vector<LittleEntry>& entries)
{
    PerKeyFrame frame{.function = littleFrameFunction, .params = {}};
    for (const auto& entry : entries) {
        frame.params.insert(frame.params.end(),
                            {entry.key, entry.color.red(), entry.color.green(), entry.color.blue()});
    }
    if (entries.size() < littleFrameMaxKeys) {
        frame.params.push_back(terminator);
    }
    return frame;
}

} // namespace

std::uint8_t g915FrameKeyCode(std::uint8_t zone, std::uint8_t index) noexcept
{
    // Aritmética de 8 bits com estouro, como o `unsigned char idx` do original.
    switch (zone) {
    case zoneGKeys:
        return static_cast<std::uint8_t>(index + 0xB3U);
    case zoneModifiers:
        return static_cast<std::uint8_t>(index - 0x78U);
    case zoneKeyboard:
        return static_cast<std::uint8_t>(index - 0x03U);
    case zoneLogo:
        return static_cast<std::uint8_t>(index + 0xD1U);
    default:
        return index;
    }
}

std::vector<PerKeyFrame> buildG915PerKeyFrames(const domain::KeyColorMap& colors)
{
    struct Group {
        domain::RgbColor color;
        std::vector<std::uint8_t> keys;
    };
    std::map<std::uint32_t, Group> byColor;
    for (const auto& [id, color] : colors) {
        const auto& definition = domain::G915KeyCatalog::definition(id);
        auto [it, inserted] = byColor.try_emplace(openRgbOrder(color), Group{.color = color, .keys = {}});
        it->second.keys.push_back(g915FrameKeyCode(definition.zone, definition.index));
    }

    std::vector<PerKeyFrame> bigFrames;
    std::vector<PerKeyFrame> littleFrames;
    std::vector<LittleEntry> pending;
    for (const auto& [order, group] : byColor) {
        for (std::size_t first = 0; first < group.keys.size(); first += bigFrameMaxKeys) {
            const std::size_t left = group.keys.size() - first;
            // Mais de 4 teclas da mesma cor: quadro grande; até 4: pares tecla/cor em quadros pequenos.
            if (left > littleFrameMaxKeys) {
                bigFrames.push_back(bigFrame(group.color, group.keys, first, std::min(left, bigFrameMaxKeys)));
                continue;
            }
            for (std::size_t i = 0; i < left; ++i) {
                pending.push_back({.key = group.keys[first + i], .color = group.color});
                if (pending.size() == littleFrameMaxKeys) {
                    littleFrames.push_back(littleFrame(pending));
                    pending.clear();
                }
            }
        }
    }
    if (!pending.empty()) {
        littleFrames.push_back(littleFrame(pending));
    }

    static_assert(3 + bigFrameMaxKeys == dataFrameSize && 4 * littleFrameMaxKeys == dataFrameSize);
    bigFrames.insert(bigFrames.end(), littleFrames.begin(), littleFrames.end());
    return bigFrames;
}

} // namespace ghub::lighting::infrastructure
