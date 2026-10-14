#pragma once

#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ghub::camera {

enum class ControlKind : std::uint8_t { Integer, Boolean, Menu };

struct MenuOption {
    std::int64_t value;
    std::string label;

    friend bool operator==(const MenuOption&, const MenuOption&) = default;
};

// Dados brutos de um controle, ainda não validados.
struct CameraControlSpec {
    std::uint32_t id; // identificador opaco do backend (no V4L2, o CID)
    std::string name;
    ControlKind kind;
    std::int64_t minimum;
    std::int64_t maximum;
    std::int64_t step;
    std::int64_t defaultValue;
    std::optional<std::int64_t> currentValue; // ausente: o driver não deixou ler
    bool inactive;
    bool readOnly;
    std::vector<MenuOption> options;
    // O firmware ajusta este valor sozinho no modo atual (ex.: ganho com exposição automática): gravar é aceito,
    // mas o valor não se sustenta.
    bool firmwareManaged = false;

    friend bool operator==(const CameraControlSpec&, const CameraControlSpec&) = default;
};

// Controle de câmera. Objeto de valor imutável; só existe válido.
class CameraControl {
public:
    [[nodiscard]] static shared_kernel::Result<CameraControl> create(CameraControlSpec spec);

    [[nodiscard]] std::uint32_t id() const noexcept { return spec_.id; }
    [[nodiscard]] const std::string& name() const noexcept { return spec_.name; }
    [[nodiscard]] ControlKind kind() const noexcept { return spec_.kind; }
    [[nodiscard]] std::int64_t minimum() const noexcept { return spec_.minimum; }
    [[nodiscard]] std::int64_t maximum() const noexcept { return spec_.maximum; }
    [[nodiscard]] std::int64_t step() const noexcept { return spec_.step; }
    [[nodiscard]] std::int64_t defaultValue() const noexcept { return spec_.defaultValue; }
    [[nodiscard]] std::optional<std::int64_t> currentValue() const noexcept { return spec_.currentValue; }
    [[nodiscard]] bool inactive() const noexcept { return spec_.inactive; }
    [[nodiscard]] bool readOnly() const noexcept { return spec_.readOnly; }
    [[nodiscard]] const std::vector<MenuOption>& options() const noexcept { return spec_.options; }
    [[nodiscard]] bool firmwareManaged() const noexcept { return spec_.firmwareManaged; }

    // Cópia com a marca de firmware alterada; o resto do controle não muda.
    [[nodiscard]] CameraControl withFirmwareManaged(bool managed) const
    {
        CameraControl copy = *this;
        copy.spec_.firmwareManaged = managed;
        return copy;
    }

    // Opção de menu cujo valor é o atual; vazio se não for Menu, sem valor atual ou sem opção correspondente.
    [[nodiscard]] std::optional<MenuOption> currentOption() const;

    // Sucesso se `value` pode ser gravado como está (sem ajuste): controle gravável e ativo, valor na faixa
    // e no passo, 0/1 para booleano, item enumerado para menu. Erro InvalidArgument com o motivo.
    [[nodiscard]] shared_kernel::Result<void> validateValue(std::int64_t value) const;

    friend bool operator==(const CameraControl&, const CameraControl&) = default;

private:
    explicit CameraControl(CameraControlSpec spec) : spec_(std::move(spec)) {}

    CameraControlSpec spec_;
};

} // namespace ghub::camera
