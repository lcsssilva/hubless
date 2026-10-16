#pragma once

#include "modules/headset/domain/HeadsetStatus.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::headset {

// Port: lê o estado do G733 (bandas do EQ e leitura não interpretada). Só leitura. O chamador
// (ReadHeadsetStatus) já segura o DeviceLease da interface; a fonte não decide sobre acesso exclusivo.
class HeadsetStatusSource {
public:
    virtual ~HeadsetStatusSource() = default;

    [[nodiscard]] virtual shared_kernel::Result<HeadsetStatus> read() = 0;

protected:
    HeadsetStatusSource() = default;
    HeadsetStatusSource(const HeadsetStatusSource&) = default;
    HeadsetStatusSource& operator=(const HeadsetStatusSource&) = default;
    HeadsetStatusSource(HeadsetStatusSource&&) = default;
    HeadsetStatusSource& operator=(HeadsetStatusSource&&) = default;
};

} // namespace ghub::headset
