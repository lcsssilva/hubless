#include "modules/device_inventory/domain/DeviceType.hpp"

#include <array>
#include <string_view>

namespace ghub::device_inventory {

std::string deviceTypeText(std::uint8_t type)
{
    static constexpr std::array<std::string_view, 8> names{
        "teclado", "controle remoto", "teclado numérico", "mouse", "touchpad", "trackball", "apresentador", "receptor",
    };
    if (type < names.size()) {
        return std::string{names.at(type)};
    }
    return "outro (" + std::to_string(type) + ")";
}

} // namespace ghub::device_inventory
