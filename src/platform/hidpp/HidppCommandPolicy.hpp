#pragma once

#include <cstdint>
#include <span>

namespace ghub::platform::hidpp {

// Política de comandos HID++ 2.0 permitidos, injetada no HidppChannel pelo módulo dono do dispositivo
// (ADR-0017 decisão 2). O canal consulta-a antes de QUALQUER write, inclusive a consulta Root GetFeature.
class HidppCommandPolicy {
public:
    virtual ~HidppCommandPolicy() = default;

    [[nodiscard]] virtual bool isAllowed(std::uint16_t featureId, std::uint8_t function,
                                         std::span<const std::uint8_t> params) const noexcept = 0;
};

} // namespace ghub::platform::hidpp
