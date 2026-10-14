#pragma once

#include "modules/battery/domain/BatteryReading.hpp"
#include "shared_kernel/Result.hpp"

#include <QByteArrayView>

#include <optional>

namespace ghub::battery {

// Converte a saída `headsetcontrol -o json` (api_version 1.x) na leitura do G733 (046d:0afe).
// nullopt = G733 não presente na saída. Falha de formato -> MalformedData (ADR-0007).
[[nodiscard]] shared_kernel::Result<std::optional<BatteryReading>> parseHeadsetControlBattery(QByteArrayView json);

} // namespace ghub::battery
