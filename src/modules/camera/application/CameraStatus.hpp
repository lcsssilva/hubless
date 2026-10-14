#pragma once

#include "modules/camera/domain/CameraDescription.hpp"
#include "shared_kernel/DomainError.hpp"

#include <cstdint>
#include <utility>
#include <variant>

namespace ghub::camera {

enum class CameraState : std::uint8_t { NotDetected, Unavailable, Ready };

// Estado da câmera: exatamente um de não detectada, indisponível (com o erro) ou pronta (com a descrição).
// Só se lê a descrição/erro do estado que a carrega.
class CameraStatus {
public:
    [[nodiscard]] static CameraStatus notDetected() { return CameraStatus{NotDetectedTag{}}; }
    [[nodiscard]] static CameraStatus unavailable(shared_kernel::DomainError error)
    {
        return CameraStatus{std::move(error)};
    }
    [[nodiscard]] static CameraStatus ready(CameraDescription description)
    {
        return CameraStatus{std::move(description)};
    }

    [[nodiscard]] CameraState state() const noexcept { return static_cast<CameraState>(value_.index()); }

    // Nulo fora do estado correspondente; o ponteiro vale enquanto este objeto existir.
    [[nodiscard]] const CameraDescription* description() const noexcept { return std::get_if<2>(&value_); }
    [[nodiscard]] const shared_kernel::DomainError* error() const noexcept { return std::get_if<1>(&value_); }

    friend bool operator==(const CameraStatus&, const CameraStatus&) = default;

private:
    struct NotDetectedTag {
        friend bool operator==(const NotDetectedTag&, const NotDetectedTag&) = default;
    };
    // A ordem das alternativas espelha CameraState.
    using Value = std::variant<NotDetectedTag, shared_kernel::DomainError, CameraDescription>;

    explicit CameraStatus(Value value) : value_(std::move(value)) {}

    Value value_;
};

} // namespace ghub::camera
