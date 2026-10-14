#include "modules/camera/infrastructure/V4l2ControlReading.hpp"

#include <cerrno>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

namespace ghub::camera::v4l2 {

namespace {

using shared_kernel::ErrorCode;
using shared_kernel::fail;
using shared_kernel::Result;

constexpr std::int64_t maxMenuItems = 256;

// Consultas do kernel (macros expandem para constantes sem casts).
constexpr unsigned long requestQueryMenu = VIDIOC_QUERYMENU;
constexpr unsigned long requestGetCtrl = VIDIOC_G_CTRL;

// Itens existentes do menu (índices de minimum a maximum; os que o driver recusa com EINVAL são pulados).
Result<std::vector<MenuOption>> readMenu(V4l2Api& api, int fd, const v4l2_query_ext_ctrl& control)
{
    if (control.minimum < 0 || control.maximum < control.minimum || control.maximum - control.minimum >= maxMenuItems) {
        return fail(ErrorCode::MalformedData, "menu '" + fixedText(control.name) + "' com faixa de índices inválida");
    }
    std::vector<MenuOption> options;
    for (std::int64_t index = control.minimum; index <= control.maximum; ++index) {
        v4l2_querymenu item = {};
        item.id = control.id;
        item.index = static_cast<std::uint32_t>(index); // 0 <= index < 256 garantido acima
        const auto exists = enumerate(api, fd, requestQueryMenu, "VIDIOC_QUERYMENU", &item);
        if (!exists.has_value()) {
            return std::unexpected(exists.error());
        }
        if (*exists) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access): v4l2_querymenu tem união no uAPI do kernel.
            options.push_back({.value = index, .label = fixedText(item.name)});
        }
    }
    return options;
}

// Valor atual: ausente se o controle é só de escrita ou o driver não deixou ler; ENODEV é erro.
Result<std::optional<std::int64_t>> readValue(V4l2Api& api, int fd, const v4l2_query_ext_ctrl& control)
{
    if ((control.flags & V4L2_CTRL_FLAG_WRITE_ONLY) != 0) {
        return std::optional<std::int64_t>{};
    }
    v4l2_control value = {};
    value.id = control.id;
    const int rc = api.ioctl(fd, requestGetCtrl, &value);
    if (rc == -ENODEV) {
        return std::unexpected(unavailable("VIDIOC_G_CTRL", rc));
    }
    if (rc != 0) {
        return std::optional<std::int64_t>{};
    }
    return std::optional<std::int64_t>{value.value};
}

} // namespace

std::string errnoText(int negativeErrno)
{
    return std::generic_category().message(-negativeErrno);
}

shared_kernel::DomainError unavailable(std::string_view what, int negativeErrno)
{
    return {.code = ErrorCode::BackendUnavailable, .detail = std::string{what} + ": " + errnoText(negativeErrno)};
}

shared_kernel::DomainError limitExceeded(std::string_view what)
{
    return {.code = ErrorCode::MalformedData, .detail = std::string{what} + ": o driver excedeu o limite de itens"};
}

std::string fixedText(std::span<const std::uint8_t> buffer)
{
    std::string text;
    for (const std::uint8_t byte : buffer) {
        if (byte == 0) {
            break;
        }
        text.push_back(static_cast<char>(byte));
    }
    return text;
}

std::string fixedText(std::span<const char> buffer)
{
    const std::string_view view{buffer.data(), buffer.size()};
    return std::string{view.substr(0, view.find('\0'))};
}

Result<bool> enumerate(V4l2Api& api, int fd, unsigned long request, std::string_view name, void* argument)
{
    const int rc = api.ioctl(fd, request, argument);
    if (rc == 0) {
        return true;
    }
    if (rc == -EINVAL) {
        return false;
    }
    return std::unexpected(unavailable(name, rc));
}

std::optional<ControlKind> supportedKind(std::uint32_t type)
{
    switch (type) {
    case V4L2_CTRL_TYPE_INTEGER:
        return ControlKind::Integer;
    case V4L2_CTRL_TYPE_BOOLEAN:
        return ControlKind::Boolean;
    case V4L2_CTRL_TYPE_MENU:
        return ControlKind::Menu;
    default:
        return std::nullopt;
    }
}

Result<CameraControl> buildControl(V4l2Api& api, int fd, const v4l2_query_ext_ctrl& control, ControlKind kind)
{
    if (control.step > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return fail(ErrorCode::MalformedData, "controle '" + fixedText(control.name) + "' com passo fora da faixa");
    }
    CameraControlSpec spec{
        .id = control.id,
        .name = fixedText(control.name),
        .kind = kind,
        .minimum = control.minimum,
        .maximum = control.maximum,
        .step = control.step == 0 ? 1 : static_cast<std::int64_t>(control.step),
        .defaultValue = control.default_value,
        .currentValue = std::nullopt,
        .inactive = (control.flags & V4L2_CTRL_FLAG_INACTIVE) != 0,
        .readOnly = (control.flags & V4L2_CTRL_FLAG_READ_ONLY) != 0,
        .options = {},
    };
    if (kind == ControlKind::Menu) {
        auto options = readMenu(api, fd, control);
        if (!options.has_value()) {
            return std::unexpected(options.error());
        }
        spec.options = std::move(*options);
    }
    auto value = readValue(api, fd, control);
    if (!value.has_value()) {
        return std::unexpected(value.error());
    }
    spec.currentValue = *value;
    auto built = CameraControl::create(std::move(spec));
    if (!built.has_value()) {
        return fail(ErrorCode::MalformedData, built.error().detail);
    }
    return built;
}

} // namespace ghub::camera::v4l2
