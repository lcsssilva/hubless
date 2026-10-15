#pragma once

#include "modules/device_inventory/domain/FirmwareVersion.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ghub::device_inventory {

struct DeviceIdentityParts {
    std::optional<std::string> name;        // nulo = "não informado"
    std::optional<std::uint8_t> deviceType; // 0x0005 fn2; nulo = "não informado"
    std::vector<FirmwareVersion> firmwares; // vazio = "não informado"
    std::vector<std::uint16_t> featureIds;  // na ordem do índice da feature no dispositivo
    bool firmwareTruncated = false;         // o dispositivo tem mais entidades do que foram lidas
    bool featuresTruncated = false;

    friend bool operator==(const DeviceIdentityParts&,
                           const DeviceIdentityParts&) = default; // o dispositivo tem mais features do que foram lidas
};

// Identidade de um dispositivo, lida por HID++ (INC-015, ADR-0024). Objeto de valor imutável, sem número de série
// nem unit ID (R3). Os limites de R6 são invariantes: listas acima deles são cortadas e o corte é marcado.
class DeviceIdentity {
public:
    static constexpr std::size_t maxNameLength = 64;
    static constexpr std::size_t maxFirmwareEntries = 8;
    static constexpr std::size_t maxFeatures = 64;

    explicit DeviceIdentity(DeviceIdentityParts parts = {});

    [[nodiscard]] const std::optional<std::string>& name() const noexcept { return parts_.name; }
    [[nodiscard]] const std::optional<std::uint8_t>& deviceType() const noexcept { return parts_.deviceType; }
    [[nodiscard]] const std::vector<FirmwareVersion>& firmwares() const noexcept { return parts_.firmwares; }
    [[nodiscard]] const std::vector<std::uint16_t>& featureIds() const noexcept { return parts_.featureIds; }
    [[nodiscard]] bool firmwareTruncated() const noexcept { return parts_.firmwareTruncated; }
    [[nodiscard]] bool featuresTruncated() const noexcept { return parts_.featuresTruncated; }

    friend bool operator==(const DeviceIdentity&, const DeviceIdentity&) = default;

private:
    DeviceIdentityParts parts_;
};

// Nome a partir dos bytes informados pelo comprimento da 0x0005 fn0 (R6): byte fora de ASCII imprimível vira '?';
// vazio ou acima de DeviceIdentity::maxNameLength -> nulo ("não informado").
[[nodiscard]] std::optional<std::string> deviceNameFromBytes(std::span<const std::uint8_t> bytes);

} // namespace ghub::device_inventory
