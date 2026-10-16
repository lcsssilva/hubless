#pragma once

#include "modules/lighting/domain/KeyboardStatus.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::lighting::application {

// Port: lê o estado do G915 (brilho e report rate). Só leitura. O chamador (ReadKeyboardStatus) já segura o
// DeviceLease da interface; a fonte não decide sobre acesso exclusivo. Sem estado entre leituras.
class KeyboardStatusSource {
public:
    virtual ~KeyboardStatusSource() = default;

    [[nodiscard]] virtual shared_kernel::Result<KeyboardStatus> read() = 0;

protected:
    KeyboardStatusSource() = default;
    KeyboardStatusSource(const KeyboardStatusSource&) = default;
    KeyboardStatusSource& operator=(const KeyboardStatusSource&) = default;
    KeyboardStatusSource(KeyboardStatusSource&&) = default;
    KeyboardStatusSource& operator=(KeyboardStatusSource&&) = default;
};

} // namespace ghub::lighting::application
