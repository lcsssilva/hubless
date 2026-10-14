#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace ghub::device_access {

// Software externo conhecido por disputar o acesso exclusivo aos dispositivos (DEVICE_ACCESS_POLICY §3).
enum class ExternalOwner : std::uint8_t { OpenRgb, Solaar, Piper, Ratbagd, HeadsetControl };

// Identifica o dono pelo nome em /proc/<pid>/comm (correspondência exata, sensível a maiúsculas).
[[nodiscard]] std::optional<ExternalOwner> externalOwnerForProcessName(std::string_view comm) noexcept;

// Nome do processo (comm) do dono externo.
[[nodiscard]] std::string_view processName(ExternalOwner owner) noexcept;

} // namespace ghub::device_access
