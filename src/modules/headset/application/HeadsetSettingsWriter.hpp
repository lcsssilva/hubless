#pragma once

#include "modules/headset/domain/HeadsetLights.hpp"
#include "modules/headset/domain/SidetoneLevel.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::headset {

// Port: aplica um ajuste no headset. Bloqueante (chamado em thread de trabalho), sem retentativa. O sucesso
// significa "comando aceito pelo backend"; o headset não confirma o valor (ADR-0015).
class HeadsetSettingsWriter {
public:
    virtual ~HeadsetSettingsWriter() = default;
    [[nodiscard]] virtual shared_kernel::Result<void> applySidetone(SidetoneLevel level) = 0;
    [[nodiscard]] virtual shared_kernel::Result<void> applyLights(HeadsetLights lights) = 0;

protected:
    HeadsetSettingsWriter() = default;
    HeadsetSettingsWriter(const HeadsetSettingsWriter&) = default;
    HeadsetSettingsWriter& operator=(const HeadsetSettingsWriter&) = default;
    HeadsetSettingsWriter(HeadsetSettingsWriter&&) = default;
    HeadsetSettingsWriter& operator=(HeadsetSettingsWriter&&) = default;
};

} // namespace ghub::headset
