#pragma once

#include "modules/camera/application/CameraBackend.hpp"

#include <optional>
#include <utility>

namespace ghub::fakes {

// Backend de câmera que devolve o resultado configurado pelo teste e conta as leituras.
class FakeCameraBackend final : public camera::CameraBackend {
public:
    [[nodiscard]] shared_kernel::Result<std::optional<camera::CameraDescription>> readCamera() override
    {
        ++calls_;
        return result_;
    }

    void setResult(shared_kernel::Result<std::optional<camera::CameraDescription>> result)
    {
        result_ = std::move(result);
    }

    [[nodiscard]] int calls() const noexcept { return calls_; }

private:
    shared_kernel::Result<std::optional<camera::CameraDescription>> result_{std::nullopt};
    int calls_ = 0;
};

} // namespace ghub::fakes
