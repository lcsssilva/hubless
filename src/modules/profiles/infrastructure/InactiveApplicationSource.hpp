#pragma once

#include "modules/profiles/application/ActiveApplicationSource.hpp"

#include <string>

namespace ghub::profiles {

// Fonte inerte (teste de fumaça): nunca observa nada e nunca toca no KWin nem no barramento de sessão. O start
// sempre devolve erro com o motivo.
class InactiveApplicationSource final : public ActiveApplicationSource {
public:
    [[nodiscard]] shared_kernel::Result<void> start(ActivationListener /*onActivated*/,
                                                    StatusListener /*onStatus*/) override
    {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   std::string("desativado no teste de fumaça"));
    }

    void stop() noexcept override {}
};

} // namespace ghub::profiles
