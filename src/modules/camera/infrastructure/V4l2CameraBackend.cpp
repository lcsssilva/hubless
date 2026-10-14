#include "modules/camera/infrastructure/V4l2CameraBackend.hpp"

#include "modules/camera/infrastructure/V4l2CaptureNode.hpp"
#include "modules/camera/infrastructure/V4l2ControlReading.hpp"

#include <linux/videodev2.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace ghub::camera {

namespace {

using shared_kernel::ErrorCode;
using shared_kernel::fail;
using shared_kernel::Result;
using v4l2::buildControl;
using v4l2::enumerate;
using v4l2::fixedText;
using v4l2::limitExceeded;
using v4l2::supportedKind;
using v4l2::unavailable;

// Limites fixos das enumerações (R11): um driver que nunca devolve EINVAL não trava a leitura.
constexpr std::uint32_t maxFormats = 64;
constexpr std::uint32_t maxSizesPerFormat = 256;
constexpr std::uint32_t maxIntervalsPerSize = 64;
constexpr std::uint32_t maxControls = 256;

// Os únicos ioctls que este adaptador usa (ADR-0008, R14). Todos são consultas.
// As macros do kernel expandem para constantes `unsigned int` sem casts: não geram avisos com
// -Wconversion/-Wold-style-cast; a conversão para `unsigned long` é um alargamento implícito.
constexpr unsigned long requestQueryCap = VIDIOC_QUERYCAP;
constexpr unsigned long requestEnumFmt = VIDIOC_ENUM_FMT;
constexpr unsigned long requestEnumFrameSizes = VIDIOC_ENUM_FRAMESIZES;
constexpr unsigned long requestEnumFrameIntervals = VIDIOC_ENUM_FRAMEINTERVALS;
constexpr unsigned long requestQueryExtCtrl = VIDIOC_QUERY_EXT_CTRL;

// Fecha o descritor em todos os caminhos de saída (RAII).
class OpenNode {
public:
    OpenNode(V4l2Api& api, int fd) noexcept : api_(api), fd_(fd) {}
    OpenNode(const OpenNode&) = delete;
    OpenNode& operator=(const OpenNode&) = delete;
    OpenNode(OpenNode&&) = delete;
    OpenNode& operator=(OpenNode&&) = delete;
    ~OpenNode() { api_.close(fd_); }

    [[nodiscard]] int fd() const noexcept { return fd_; }

private:
    V4l2Api& api_;
    int fd_;
};

bool hasCaptureCapability(const v4l2_capability& capability)
{
    return (nodeCapabilities(capability) & V4L2_CAP_VIDEO_CAPTURE) != 0;
}

// FourCC: byte menos significativo primeiro.
std::string fourccText(std::uint32_t pixelFormat)
{
    std::string text;
    for (unsigned shift = 0; shift < 32; shift += 8) {
        text.push_back(static_cast<char>((pixelFormat >> shift) & 0xFFU));
    }
    return text;
}

// Taxas discretas de um tamanho; pode voltar vazio (intervalo STEPWISE na primeira posição).
Result<std::vector<FrameRate>> readRates(V4l2Api& api, int fd, std::uint32_t pixelFormat, std::uint32_t width,
                                         std::uint32_t height)
{
    std::vector<FrameRate> rates;
    for (std::uint32_t index = 0;; ++index) {
        v4l2_frmivalenum interval = {};
        interval.index = index;
        interval.pixel_format = pixelFormat;
        interval.width = width;
        interval.height = height;
        const auto more = enumerate(api, fd, requestEnumFrameIntervals, "VIDIOC_ENUM_FRAMEINTERVALS", &interval);
        if (!more.has_value()) {
            return std::unexpected(more.error());
        }
        if (!*more || interval.type != V4L2_FRMIVAL_TYPE_DISCRETE) {
            return rates; // fim da lista ou intervalo contínuo/em passos (ignorado, ADR-0008 §5)
        }
        if (index >= maxIntervalsPerSize) {
            return std::unexpected(limitExceeded("VIDIOC_ENUM_FRAMEINTERVALS"));
        }
        // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access): v4l2_frmivalenum é uma união do uAPI do kernel;
        // `discrete` é o membro válido porque type == V4L2_FRMIVAL_TYPE_DISCRETE (verificado acima).
        const std::uint32_t numerator = interval.discrete.numerator;
        const std::uint32_t denominator = interval.discrete.denominator;
        // NOLINTEND(cppcoreguidelines-pro-type-union-access)
        if (numerator == 0 || denominator == 0) {
            return fail(ErrorCode::MalformedData, "intervalo de quadros com numerador ou denominador zero");
        }
        // Intervalo = segundos por quadro; taxa = quadros por segundo.
        const auto rate = FrameRate::create(denominator, numerator);
        if (!rate.has_value()) {
            return fail(ErrorCode::MalformedData, rate.error().detail);
        }
        rates.push_back(*rate);
    }
}

// Modos (um por tamanho discreto com ao menos uma taxa discreta) de um formato.
Result<std::vector<VideoMode>> readModesOf(V4l2Api& api, int fd, std::uint32_t pixelFormat)
{
    const auto format = PixelFormat::create(fourccText(pixelFormat));
    if (!format.has_value()) {
        return fail(ErrorCode::MalformedData, "FourCC inválido: " + format.error().detail);
    }
    std::vector<VideoMode> modes;
    for (std::uint32_t index = 0;; ++index) {
        v4l2_frmsizeenum size = {};
        size.index = index;
        size.pixel_format = pixelFormat;
        const auto more = enumerate(api, fd, requestEnumFrameSizes, "VIDIOC_ENUM_FRAMESIZES", &size);
        if (!more.has_value()) {
            return std::unexpected(more.error());
        }
        if (!*more || size.type != V4L2_FRMSIZE_TYPE_DISCRETE) {
            return modes; // fim da lista ou tamanhos contínuos/em passos (ignorados, ADR-0008 §5)
        }
        if (index >= maxSizesPerFormat) {
            return std::unexpected(limitExceeded("VIDIOC_ENUM_FRAMESIZES"));
        }
        // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access): v4l2_frmsizeenum é uma união do uAPI do kernel;
        // `discrete` é o membro válido porque type == V4L2_FRMSIZE_TYPE_DISCRETE (verificado acima).
        const std::uint32_t width = size.discrete.width;
        const std::uint32_t height = size.discrete.height;
        // NOLINTEND(cppcoreguidelines-pro-type-union-access)
        auto rates = readRates(api, fd, pixelFormat, width, height);
        if (!rates.has_value()) {
            return std::unexpected(rates.error());
        }
        if (rates->empty()) {
            continue;
        }
        auto mode = VideoMode::create(*format, width, height, std::move(*rates));
        if (!mode.has_value()) {
            return fail(ErrorCode::MalformedData, mode.error().detail);
        }
        modes.push_back(std::move(*mode));
    }
}

Result<std::vector<VideoMode>> readModes(V4l2Api& api, int fd)
{
    std::vector<VideoMode> modes;
    for (std::uint32_t index = 0;; ++index) {
        v4l2_fmtdesc description = {};
        description.index = index;
        description.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        const auto more = enumerate(api, fd, requestEnumFmt, "VIDIOC_ENUM_FMT", &description);
        if (!more.has_value()) {
            return std::unexpected(more.error());
        }
        if (!*more) {
            return modes;
        }
        if (index >= maxFormats) {
            return std::unexpected(limitExceeded("VIDIOC_ENUM_FMT"));
        }
        auto ofFormat = readModesOf(api, fd, description.pixelformat);
        if (!ofFormat.has_value()) {
            return std::unexpected(ofFormat.error());
        }
        std::ranges::move(*ofFormat, std::back_inserter(modes));
    }
}

Result<std::vector<CameraControl>> readControls(V4l2Api& api, int fd)
{
    std::vector<CameraControl> controls;
    std::uint32_t lastId = 0;
    for (std::uint32_t count = 0;; ++count) {
        v4l2_query_ext_ctrl control = {};
        control.id = lastId | V4L2_CTRL_FLAG_NEXT_CTRL;
        const auto more = enumerate(api, fd, requestQueryExtCtrl, "VIDIOC_QUERY_EXT_CTRL", &control);
        if (!more.has_value()) {
            return std::unexpected(more.error());
        }
        if (!*more) {
            return controls;
        }
        if (count >= maxControls) {
            return std::unexpected(limitExceeded("VIDIOC_QUERY_EXT_CTRL"));
        }
        if (control.id <= lastId) {
            return fail(ErrorCode::MalformedData, "VIDIOC_QUERY_EXT_CTRL: ids de controle não crescentes");
        }
        lastId = control.id;
        const auto kind = supportedKind(control.type);
        if (!kind.has_value() || (control.flags & V4L2_CTRL_FLAG_DISABLED) != 0) {
            continue; // fora do que o produto entende (ADR-0008 §5)
        }
        auto built = buildControl(api, fd, control, *kind);
        if (!built.has_value()) {
            return std::unexpected(built.error());
        }
        controls.push_back(std::move(*built));
    }
}

// Lê o que há no nó de captura já aberto.
Result<CameraDescription> describe(V4l2Api& api, int fd, const v4l2_capability& capability)
{
    CameraDescription description;
    description.name = fixedText(capability.card);
    if (description.name.empty()) {
        return fail(ErrorCode::MalformedData, "o driver não informou o nome da câmera");
    }
    auto modes = readModes(api, fd);
    if (!modes.has_value()) {
        return std::unexpected(modes.error());
    }
    description.modes = std::move(*modes);
    auto controls = readControls(api, fd);
    if (!controls.has_value()) {
        return std::unexpected(controls.error());
    }
    description.controls = std::move(*controls);
    return description;
}

} // namespace

V4l2CameraBackend::V4l2CameraBackend(device_inventory::UsbId target, V4l2Api& api, std::filesystem::path sysfsRoot,
                                     std::filesystem::path devRoot)
    : target_(target), api_(&api), sysfsRoot_(std::move(sysfsRoot)), devRoot_(std::move(devRoot))
{
}

shared_kernel::Result<std::optional<CameraDescription>> V4l2CameraBackend::readCamera()
{
    const auto candidates = findCandidateNodes(sysfsRoot_, target_);
    if (!candidates.has_value()) {
        return std::unexpected(candidates.error());
    }
    for (const std::string& name : *candidates) {
        const std::filesystem::path node = devRoot_ / name;
        const int fd = api_->open(node);
        if (fd < 0) {
            if (fd == -ENOENT || fd == -ENODEV || fd == -ENXIO) {
                continue; // o nó sumiu entre o sysfs e o open
            }
            return std::unexpected(unavailable(node.string(), fd));
        }
        const OpenNode opened{*api_, fd};

        v4l2_capability capability = {};
        if (const int rc = api_->ioctl(fd, requestQueryCap, &capability); rc != 0) {
            return std::unexpected(unavailable("VIDIOC_QUERYCAP", rc));
        }
        if (!hasCaptureCapability(capability)) {
            continue; // nó de metadados ou de outra função
        }
        auto description = describe(*api_, fd, capability);
        if (!description.has_value()) {
            return std::unexpected(description.error());
        }
        return std::optional<CameraDescription>{std::move(*description)};
    }
    return std::nullopt;
}

} // namespace ghub::camera
