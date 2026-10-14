#include "modules/camera/domain/CameraControl.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>

namespace ghub::camera {

namespace {

shared_kernel::Result<CameraControl> invalid(const CameraControlSpec& spec, std::string_view rule)
{
    return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                               "controle '" + spec.name + "': " + std::string{rule});
}

bool inRange(const CameraControlSpec& spec, std::int64_t value)
{
    return value >= spec.minimum && value <= spec.maximum;
}

// Regras das opções de menu; devolve a descrição da violação ou vazio.
std::string_view optionsViolation(const CameraControlSpec& spec)
{
    if (spec.kind != ControlKind::Menu) {
        return spec.options.empty() ? "" : "opções só existem em controles de menu";
    }
    if (spec.options.empty()) {
        return "menu exige ao menos uma opção";
    }
    for (auto current = spec.options.begin(); current != spec.options.end(); ++current) {
        const auto& option = *current;
        if (!inRange(spec, option.value)) {
            return "valor de opção fora da faixa";
        }
        if (option.label.empty()) {
            return "opção com rótulo vazio";
        }
        const auto isSameValue = [&option](const MenuOption& other) { return other.value == option.value; };
        if (std::ranges::any_of(std::next(current), spec.options.end(), isSameValue)) {
            return "valores de opção repetidos";
        }
    }
    return "";
}

} // namespace

shared_kernel::Result<CameraControl> CameraControl::create(CameraControlSpec spec)
{
    if (spec.name.empty()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "controle sem nome");
    }
    if (spec.minimum > spec.maximum) {
        return invalid(spec, "mínimo maior que o máximo");
    }
    if (spec.step < 1) {
        return invalid(spec, "passo menor que 1");
    }
    if (!inRange(spec, spec.defaultValue)) {
        return invalid(spec, "valor padrão fora da faixa");
    }
    if (spec.currentValue.has_value() && !inRange(spec, *spec.currentValue)) {
        return invalid(spec, "valor atual fora da faixa");
    }
    if (spec.kind == ControlKind::Boolean && (spec.minimum != 0 || spec.maximum != 1)) {
        return invalid(spec, "controle booleano exige faixa de 0 a 1");
    }
    if (const auto violation = optionsViolation(spec); !violation.empty()) {
        return invalid(spec, violation);
    }
    return CameraControl{std::move(spec)};
}

shared_kernel::Result<void> CameraControl::validateValue(std::int64_t value) const
{
    const auto refuse = [this, value](std::string_view reason) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "controle '" + spec_.name + "': valor " +
                                                                                  std::to_string(value) +
                                                                                  " recusado: " + std::string{reason});
    };
    if (spec_.readOnly) {
        return refuse("controle somente leitura");
    }
    if (spec_.inactive) {
        return refuse("controle inativo");
    }
    if (!inRange(spec_, value)) {
        return refuse("fora da faixa de " + std::to_string(spec_.minimum) + " a " + std::to_string(spec_.maximum));
    }
    switch (spec_.kind) {
    case ControlKind::Integer:
        if ((value - spec_.minimum) % spec_.step != 0) {
            return refuse("fora do passo " + std::to_string(spec_.step));
        }
        break;
    case ControlKind::Boolean:
        break; // a faixa 0..1 já foi checada; create() a exige para booleanos
    case ControlKind::Menu:
        if (!std::ranges::contains(spec_.options, value, &MenuOption::value)) {
            return refuse("opção de menu inexistente");
        }
        break;
    }
    return {};
}

std::optional<MenuOption> CameraControl::currentOption() const
{
    if (spec_.kind != ControlKind::Menu || !spec_.currentValue.has_value()) {
        return std::nullopt;
    }
    const auto found = std::ranges::find(spec_.options, *spec_.currentValue, &MenuOption::value);
    if (found == spec_.options.end()) {
        return std::nullopt;
    }
    return *found;
}

} // namespace ghub::camera
