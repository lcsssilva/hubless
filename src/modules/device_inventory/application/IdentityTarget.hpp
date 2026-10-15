#pragma once

#include <cstdint>

namespace ghub::device_inventory {

// Dispositivos cuja identidade se lê por HID++ (INC-015). A Brio 100 não fala HID++.
enum class IdentityTarget : std::uint8_t {
    G915,
    G703,
    G733,
};

} // namespace ghub::device_inventory
