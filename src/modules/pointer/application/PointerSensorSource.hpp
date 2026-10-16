#pragma once

#include "modules/pointer/domain/PointerSensorStatus.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::pointer {

// Port: lê o estado do sensor do G703 (lista de DPI, DPI atual/padrão e report rate). Só leitura. O chamador
// (ReadPointerSensorStatus) já segura o DeviceLease da interface; a fonte não decide sobre acesso exclusivo.
class PointerSensorSource {
public:
    virtual ~PointerSensorSource() = default;

    [[nodiscard]] virtual shared_kernel::Result<PointerSensorStatus> read() = 0;

protected:
    PointerSensorSource() = default;
    PointerSensorSource(const PointerSensorSource&) = default;
    PointerSensorSource& operator=(const PointerSensorSource&) = default;
    PointerSensorSource(PointerSensorSource&&) = default;
    PointerSensorSource& operator=(PointerSensorSource&&) = default;
};

} // namespace ghub::pointer
