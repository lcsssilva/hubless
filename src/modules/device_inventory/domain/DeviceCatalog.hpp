#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ghub::device_inventory {

// VID:PID como o kernel registra o dispositivo (PID sem fio quando o dispositivo é filho de um
// receptor Lightspeed).
struct UsbId {
    std::uint16_t vendorId;
    std::uint16_t productId;

    friend auto operator<=>(const UsbId&, const UsbId&) = default;
};

// Modelos de dispositivo que o produto conhece (ADR-0006). A Brio 100 (046d:094c) entrou com PID
// confirmado em evidence/2026-10-13-testes-fisicos-cabo-g733-brio100.md.
enum class DeviceModel : std::uint8_t {
    G915,
    G703,
    G733,
    Brio100,
};

inline constexpr std::array kDeviceModels{
    DeviceModel::G915,
    DeviceModel::G703,
    DeviceModel::G733,
    DeviceModel::Brio100,
};

// Caminho pelo qual o kernel enumerou o dispositivo. A ordem dos enumeradores é a ordem de exibição.
enum class ConnectionPath : std::uint8_t {
    LightspeedReceiver,
    UsbCable,
    UsbDongle,
};

struct CatalogEntry {
    DeviceModel model;
    ConnectionPath path;

    friend bool operator==(const CatalogEntry&, const CatalogEntry&) = default;
};

// Identifica somente IDs observados nesta base (evidence/2026-10-13-enumeracao-usb.md,
// evidence/2026-10-13-g703-cabo-uevent.md, evidence/2026-10-13-g915-cabo.md); receptores não são dispositivos do catálogo.
[[nodiscard]] std::optional<CatalogEntry> identifyDevice(UsbId id) noexcept;

// Nome comercial curto do modelo (ex.: "G915").
[[nodiscard]] std::string_view modelName(DeviceModel model) noexcept;

// Nome de dispositivo HID do kernel, "%04X:%04X:%04X.%04X" (bus, VID, PID, instância;
// hid-core.c:3022 @ v7.2.9), ex.: "0003:046D:407C.000A". Qualquer outro formato → vazio.
[[nodiscard]] std::optional<UsbId> parseHidDeviceName(std::string_view name) noexcept;

// Modalias de dispositivo/interface USB do kernel, "usb:v<VID>p<PID>..." (4 hex cada; o restante é
// ignorado; `\n` final tolerado). Qualquer outro formato (pci:, platform:, hid:...) → vazio.
[[nodiscard]] std::optional<UsbId> parseUsbModalias(std::string_view modalias) noexcept;

} // namespace ghub::device_inventory
