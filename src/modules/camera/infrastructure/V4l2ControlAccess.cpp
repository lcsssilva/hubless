#include "modules/camera/infrastructure/V4l2ControlAccess.hpp"

#include "modules/camera/infrastructure/V4l2CaptureNode.hpp"
#include "modules/camera/infrastructure/V4l2ControlReading.hpp"

#include <linux/videodev2.h>

#include <cerrno>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace ghub::camera {

namespace {

using shared_kernel::ErrorCode;
using shared_kernel::fail;
using shared_kernel::Result;

// Os únicos ioctls deste adaptador (ADR-0010): QUERYCAP, QUERY_EXT_CTRL, QUERYMENU e G_CTRL (em
// V4l2ControlReading), mais S_CTRL, a única escrita do produto no dispositivo.
constexpr unsigned long requestQueryCap = VIDIOC_QUERYCAP;
constexpr unsigned long requestQueryExtCtrl = VIDIOC_QUERY_EXT_CTRL;
constexpr unsigned long requestSetCtrl = VIDIOC_S_CTRL;

// Bits de enumeração que não fazem parte do identificador de um controle.
constexpr std::uint32_t enumerationFlags = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;

// Sessão com o nó aberto. O descritor fecha no destrutor, em qualquer caminho (RAII).
class V4l2ControlSession final : public CameraControlSession {
public:
    V4l2ControlSession(V4l2Api& api, int fd) noexcept : api_(&api), fd_(fd) {}
    V4l2ControlSession(const V4l2ControlSession&) = delete;
    V4l2ControlSession& operator=(const V4l2ControlSession&) = delete;
    V4l2ControlSession(V4l2ControlSession&&) = delete;
    V4l2ControlSession& operator=(V4l2ControlSession&&) = delete;
    ~V4l2ControlSession() override { api_->close(fd_); }

    [[nodiscard]] Result<CameraControl> readControl(std::uint32_t id) override
    {
        if ((id & enumerationFlags) != 0) {
            return fail(ErrorCode::InvalidArgument, "identificador de controle inválido");
        }
        v4l2_query_ext_ctrl query = {};
        query.id = id; // id exato, sem NEXT_CTRL: não enumera
        const int rc = api_->ioctl(fd_, requestQueryExtCtrl, &query);
        if (rc == -EINVAL) {
            return fail(ErrorCode::InvalidArgument, "controle inexistente");
        }
        if (rc != 0) {
            return std::unexpected(v4l2::unavailable("VIDIOC_QUERY_EXT_CTRL", rc));
        }
        const auto kind = v4l2::supportedKind(query.type);
        if (!kind.has_value() || (query.flags & V4L2_CTRL_FLAG_DISABLED) != 0) {
            return fail(ErrorCode::InvalidArgument, "controle não suportado pelo produto");
        }
        return v4l2::buildControl(*api_, fd_, query, *kind);
    }

    [[nodiscard]] Result<void> writeControl(std::uint32_t id, std::int64_t value) override
    {
        if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max()) {
            return fail(ErrorCode::InvalidArgument, "valor fora da faixa de 32 bits");
        }
        v4l2_control control = {};
        control.id = id;
        control.value = static_cast<std::int32_t>(value);
        const int rc = api_->ioctl(fd_, requestSetCtrl, &control);
        if (rc == 0) {
            return {};
        }
        switch (rc) {
        case -ENODEV:
            return fail(ErrorCode::BackendUnavailable, "VIDIOC_S_CTRL: câmera removida");
        case -EBUSY:
            return fail(ErrorCode::DeviceBusy, "VIDIOC_S_CTRL: " + v4l2::errnoText(rc));
        case -EACCES:
        case -ERANGE:
        case -EINVAL:
            return fail(ErrorCode::InvalidArgument, "VIDIOC_S_CTRL: " + v4l2::errnoText(rc));
        default:
            return std::unexpected(v4l2::unavailable("VIDIOC_S_CTRL", rc));
        }
    }

private:
    V4l2Api* api_;
    int fd_;
};

// O nó fechado se `keep` não for chamado antes do fim do escopo.
class NodeGuard {
public:
    NodeGuard(V4l2Api& api, int fd) noexcept : api_(&api), fd_(fd) {}
    NodeGuard(const NodeGuard&) = delete;
    NodeGuard& operator=(const NodeGuard&) = delete;
    NodeGuard(NodeGuard&&) = delete;
    NodeGuard& operator=(NodeGuard&&) = delete;
    ~NodeGuard()
    {
        if (fd_ >= 0) {
            api_->close(fd_);
        }
    }

    // Entrega o descritor a quem assume o fechamento.
    int keep() noexcept { return std::exchange(fd_, -1); }

private:
    V4l2Api* api_;
    int fd_;
};

} // namespace

V4l2ControlAccess::V4l2ControlAccess(device_inventory::UsbId target, V4l2Api& api, std::filesystem::path sysfsRoot,
                                     std::filesystem::path devRoot)
    : target_(target), api_(&api), sysfsRoot_(std::move(sysfsRoot)), devRoot_(std::move(devRoot))
{
}

Result<std::unique_ptr<CameraControlSession>> V4l2ControlAccess::open()
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
            return std::unexpected(v4l2::unavailable(node.string(), fd));
        }
        NodeGuard guard{*api_, fd};
        v4l2_capability capability = {};
        if (const int rc = api_->ioctl(fd, requestQueryCap, &capability); rc != 0) {
            return std::unexpected(v4l2::unavailable("VIDIOC_QUERYCAP", rc));
        }
        if ((nodeCapabilities(capability) & V4L2_CAP_VIDEO_CAPTURE) == 0) {
            continue; // nó de metadados ou de outra função
        }
        return std::make_unique<V4l2ControlSession>(*api_, guard.keep());
    }
    return fail(ErrorCode::BackendUnavailable, "câmera não detectada");
}

} // namespace ghub::camera
