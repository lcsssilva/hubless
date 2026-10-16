#pragma once

#include "modules/headset/domain/HeadsetLighting.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::headset {

// Port: aplica cor/efeito de uma zona do headset (INC-018). Bloqueante (thread de trabalho), sem retentativa. O
// sucesso significa "comando aceito"; o efeito é volátil (o headset volta ao padrão ao religar, C-01).
class HeadsetLightingWriter {
public:
    virtual ~HeadsetLightingWriter() = default;
    [[nodiscard]] virtual shared_kernel::Result<void> applyLighting(HeadsetLighting lighting) = 0;

protected:
    HeadsetLightingWriter() = default;
    HeadsetLightingWriter(const HeadsetLightingWriter&) = default;
    HeadsetLightingWriter& operator=(const HeadsetLightingWriter&) = default;
    HeadsetLightingWriter(HeadsetLightingWriter&&) = default;
    HeadsetLightingWriter& operator=(HeadsetLightingWriter&&) = default;
};

} // namespace ghub::headset
