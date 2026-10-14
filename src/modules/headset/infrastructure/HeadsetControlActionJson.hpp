#pragma once

#include "shared_kernel/Result.hpp"

#include <QByteArray>

#include <string_view>

namespace ghub::headset {

// Interpreta a saída `headsetcontrol -o json` (api_version 1.x) de uma ação de escrita.
// `capability` é o nome do enum (ex.: "CAP_SIDETONE", "CAP_LIGHTS").
[[nodiscard]] shared_kernel::Result<void> parseHeadsetControlAction(const QByteArray& json,
                                                                    std::string_view capability);

} // namespace ghub::headset
