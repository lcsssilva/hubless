#pragma once

#include "modules/lighting/infrastructure/HidppError.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

namespace ghub::lighting::infrastructure {

// Resumo de um comando HID++ enviado e do seu resultado (INC-008 R12). De propósito NÃO há campo para os bytes dos
// parâmetros: cores e demais valores enviados nunca chegam ao rastreio.
struct HidppCommandTrace {
    std::uint8_t deviceIndex;
    std::uint16_t featureId;
    std::uint8_t function;
    std::size_t paramCount;
    std::optional<HidppErrorKind> failure; // nullopt = sucesso
    std::uint8_t errorCode = 0;            // código do dispositivo nos erros HID++ 1.0/2.0
};

// Receptor injetado no canal; vazio = sem rastreio.
using HidppTraceSink = std::function<void(const HidppCommandTrace&)>;

} // namespace ghub::lighting::infrastructure
