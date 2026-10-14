#include "modules/camera/infrastructure/V4l2VideoCapture.hpp"

#include "modules/camera/infrastructure/V4l2CaptureNode.hpp"

#include <linux/videodev2.h>
#include <poll.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
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

// Os únicos ioctls que este adaptador usa (ADR-0009, R12). Nenhum escreve controle nem intervalo de quadros.
// As macros do kernel expandem para constantes `unsigned int` sem casts: não geram avisos com
// -Wconversion/-Wold-style-cast; a conversão para `unsigned long` é um alargamento implícito.
constexpr unsigned long requestQueryCap = VIDIOC_QUERYCAP;
constexpr unsigned long requestSetFormat = VIDIOC_S_FMT;
constexpr unsigned long requestRequestBuffers = VIDIOC_REQBUFS;
constexpr unsigned long requestQueryBuffer = VIDIOC_QUERYBUF;
constexpr unsigned long requestQueueBuffer = VIDIOC_QBUF;
constexpr unsigned long requestDequeueBuffer = VIDIOC_DQBUF;
constexpr unsigned long requestStreamOn = VIDIOC_STREAMON;

// Fila de captura: pede 4 buffers (margem para o driver e para a cópia) e exige pelo menos 2; o tamanho de cada
// buffer é limitado para que um driver defeituoso não leve a um mapeamento absurdo.
// Eventos do poll como unsigned: os `revents` voltam como int, e as macros da libc são int.
constexpr unsigned pollIn = POLLIN;
constexpr unsigned pollError = POLLERR;
constexpr unsigned pollHangup = POLLHUP;
constexpr unsigned pollInvalid = POLLNVAL;

constexpr std::uint32_t requestedBufferCount = 4;
constexpr std::uint32_t minBufferCount = 2;
constexpr std::uint32_t maxBufferCount = 32;
constexpr std::uint32_t maxBufferBytes = 64U * 1024U * 1024U;

std::string errnoText(int negativeErrno)
{
    return std::generic_category().message(-negativeErrno);
}

shared_kernel::DomainError unavailable(std::string_view what, int negativeErrno)
{
    return {.code = ErrorCode::BackendUnavailable, .detail = std::string{what} + ": " + errnoText(negativeErrno)};
}

// FourCC (4 caracteres, garantido por PixelFormat) como código do V4L2: primeiro caractere no byte menos significativo.
std::uint32_t pixelFormatCode(std::string_view fourcc)
{
    std::uint32_t code = 0;
    unsigned shift = 0;
    for (const char character : fourcc) {
        code |= static_cast<std::uint32_t>(static_cast<unsigned char>(character)) << shift;
        shift += 8;
    }
    return code;
}

// -EBUSY: outra aplicação é dona da fila (ou do stream); qualquer outro erro: backend indisponível.
shared_kernel::DomainError stepError(std::string_view step, int negativeErrno)
{
    if (negativeErrno == -EBUSY) {
        return {
            .code = ErrorCode::DeviceBusy,
            .detail = std::string{step} + ": a câmera está em uso por outro programa",
        };
    }
    return unavailable(step, negativeErrno);
}

// Entrega o buffer `index` ao driver para ser preenchido (só tipo, memória e índice; sem herdar campos de DQBUF).
int queueBuffer(V4l2Api& api, int fd, std::uint32_t index)
{
    v4l2_buffer buffer = {};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = index;
    return api.ioctl(fd, requestQueueBuffer, &buffer);
}

// Sessão de captura: dona do descritor e dos mapeamentos; o destrutor desfaz tudo (RAII), inclusive quando a
// sessão ainda estava sendo montada por start().
class CaptureSession final : public VideoCaptureSession {
public:
    CaptureSession(V4l2Api& api, int fd) noexcept : api_(api), fd_(fd) {}
    CaptureSession(const CaptureSession&) = delete;
    CaptureSession& operator=(const CaptureSession&) = delete;
    CaptureSession(CaptureSession&&) = delete;
    CaptureSession& operator=(CaptureSession&&) = delete;
    ~CaptureSession() override
    {
        if (streaming_) {
            int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            (void)api_.ioctl(fd_, requestStreamOff, &type); // erro ignorado: o close também encerra o stream
        }
        for (const Mapping& mapping : mappings_) {
            api_.unmapBuffer(mapping.address, mapping.length);
        }
        api_.close(fd_);
    }

    [[nodiscard]] int fd() const noexcept { return fd_; }
    void adopt(const std::byte* address, std::size_t length)
    {
        mappings_.push_back({.address = address, .length = length});
    }
    void markStreaming() noexcept { streaming_ = true; }

    // Depois do primeiro erro a sessão não toca mais no descritor: devolve sempre o mesmo erro.
    [[nodiscard]] Result<bool> readFrame(std::chrono::milliseconds timeout, std::vector<std::byte>& frame) override
    {
        if (failure_.has_value()) {
            return std::unexpected(*failure_);
        }
        auto result = readOnce(timeout, frame);
        if (!result.has_value()) {
            failure_ = result.error();
        }
        return result;
    }

private:
    static constexpr unsigned long requestStreamOff = VIDIOC_STREAMOFF;

    struct Mapping {
        const std::byte* address;
        std::size_t length;
    };

    Result<bool> readOnce(std::chrono::milliseconds timeout, std::vector<std::byte>& frame)
    {
        const int polled = api_.poll(fd_, timeout);
        if (polled < 0) {
            return std::unexpected(unavailable("poll", polled));
        }
        const auto events = static_cast<unsigned>(polled);
        if ((events & pollIn) == 0) {
            if ((events & (pollError | pollHangup | pollInvalid)) != 0) {
                return fail(ErrorCode::BackendUnavailable, "poll: o stream foi encerrado");
            }
            return false; // prazo esgotado
        }

        v4l2_buffer buffer = {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        const int rc = api_.ioctl(fd_, requestDequeueBuffer, &buffer);
        if (rc == -EAGAIN) {
            return false; // o poll acordou, mas o driver ainda não tem quadro pronto
        }
        if (rc != 0) {
            return std::unexpected(unavailable("VIDIOC_DQBUF", rc));
        }
        if (buffer.index >= mappings_.size()) {
            return fail(ErrorCode::MalformedData, "VIDIOC_DQBUF: índice de buffer fora do que foi mapeado");
        }
        const Mapping& mapping = *std::next(mappings_.begin(), buffer.index);
        // Quadro com defeito (o uvcvideo os entrega marcados em vez de descartá-los), vazio ou maior que o
        // buffer: não vira quadro, mas o buffer volta para o driver.
        const bool usable =
            (buffer.flags & V4L2_BUF_FLAG_ERROR) == 0 && buffer.bytesused != 0 && buffer.bytesused <= mapping.length;
        if (usable) {
            const std::span<const std::byte> bytes{mapping.address, buffer.bytesused};
            frame.assign(bytes.begin(), bytes.end());
        }
        if (const int queued = queueBuffer(api_, fd_, buffer.index); queued != 0) {
            return std::unexpected(unavailable("VIDIOC_QBUF", queued));
        }
        return usable;
    }

    V4l2Api& api_;
    int fd_;
    bool streaming_ = false;
    std::vector<Mapping> mappings_;
    std::optional<shared_kernel::DomainError> failure_;
};

// Nó de captura da câmera: o primeiro "video<N>" do alvo com VIDEO_CAPTURE e STREAMING (ignora o de metadados).
// Os nós são consultados com open somente leitura; o de streaming só é aberto depois de escolhido.
// Vazio se não houver nenhum.
Result<std::optional<std::filesystem::path>> locateCaptureNode(V4l2Api& api, const std::filesystem::path& sysfsRoot,
                                                               const std::filesystem::path& devRoot,
                                                               device_inventory::UsbId target)
{
    const auto candidates = findCandidateNodes(sysfsRoot, target);
    if (!candidates.has_value()) {
        return std::unexpected(candidates.error());
    }
    constexpr std::uint32_t needed = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
    for (const std::string& name : *candidates) {
        const std::filesystem::path path = devRoot / name;
        const int probe = api.open(path);
        if (probe < 0) {
            if (probe == -ENOENT || probe == -ENODEV || probe == -ENXIO) {
                continue; // o nó sumiu entre o sysfs e o open
            }
            return std::unexpected(unavailable(path.string(), probe));
        }
        v4l2_capability capability = {};
        const int rc = api.ioctl(probe, requestQueryCap, &capability);
        api.close(probe);
        if (rc != 0) {
            return std::unexpected(unavailable("VIDIOC_QUERYCAP", rc));
        }
        if ((nodeCapabilities(capability) & needed) == needed) {
            return std::optional<std::filesystem::path>{path};
        }
    }
    return std::optional<std::filesystem::path>{};
}

} // namespace

V4l2VideoCapture::V4l2VideoCapture(device_inventory::UsbId target, V4l2Api& api, std::filesystem::path sysfsRoot,
                                   std::filesystem::path devRoot)
    : target_(target), api_(&api), sysfsRoot_(std::move(sysfsRoot)), devRoot_(std::move(devRoot))
{
}

Result<std::unique_ptr<VideoCaptureSession>> V4l2VideoCapture::start(const PreviewMode& mode)
{
    const auto node = locateCaptureNode(*api_, sysfsRoot_, devRoot_, target_);
    if (!node.has_value()) {
        return std::unexpected(node.error());
    }
    if (!node->has_value()) {
        return fail(ErrorCode::BackendUnavailable, "câmera não detectada");
    }
    const int fd = api_->openForStreaming(**node);
    if (fd < 0) {
        return std::unexpected(stepError((*node)->string(), fd));
    }
    auto session = std::make_unique<CaptureSession>(*api_, fd);

    v4l2_format format = {};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    const std::uint32_t wantedPixelFormat = pixelFormatCode(mode.format.fourcc());
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access): v4l2_format é uma união do uAPI do kernel; `pix` é o
    // membro válido para V4L2_BUF_TYPE_VIDEO_CAPTURE.
    format.fmt.pix.width = mode.width;
    format.fmt.pix.height = mode.height;
    format.fmt.pix.pixelformat = wantedPixelFormat;
    format.fmt.pix.field = V4L2_FIELD_ANY;
    if (const int rc = api_->ioctl(fd, requestSetFormat, &format); rc != 0) {
        return std::unexpected(stepError("VIDIOC_S_FMT", rc));
    }
    // O uvcvideo ajusta o pedido para o formato mais próximo sem devolver erro: nunca transmitir em outro modo.
    if (format.fmt.pix.pixelformat != wantedPixelFormat || format.fmt.pix.width != mode.width ||
        format.fmt.pix.height != mode.height) {
        return fail(ErrorCode::BackendUnavailable, "VIDIOC_S_FMT: o driver não aceitou o modo pedido");
    }
    // NOLINTEND(cppcoreguidelines-pro-type-union-access)

    v4l2_requestbuffers request = {};
    request.count = requestedBufferCount;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;
    if (const int rc = api_->ioctl(fd, requestRequestBuffers, &request); rc != 0) {
        return std::unexpected(stepError("VIDIOC_REQBUFS", rc));
    }
    if (request.count < minBufferCount) {
        return fail(ErrorCode::BackendUnavailable,
                    "VIDIOC_REQBUFS: o driver concedeu buffers de menos (" + std::to_string(request.count) + ")");
    }
    if (request.count > maxBufferCount) {
        return fail(ErrorCode::MalformedData,
                    "VIDIOC_REQBUFS: o driver concedeu buffers demais (" + std::to_string(request.count) + ")");
    }

    for (std::uint32_t index = 0; index < request.count; ++index) {
        v4l2_buffer buffer = {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = index;
        if (const int rc = api_->ioctl(fd, requestQueryBuffer, &buffer); rc != 0) {
            return std::unexpected(stepError("VIDIOC_QUERYBUF", rc));
        }
        if (buffer.index != index) {
            return fail(ErrorCode::MalformedData, "VIDIOC_QUERYBUF: índice de buffer incoerente");
        }
        if (buffer.length == 0 || buffer.length > maxBufferBytes) {
            return fail(ErrorCode::MalformedData,
                        "VIDIOC_QUERYBUF: tamanho de buffer inválido (" + std::to_string(buffer.length) + " bytes)");
        }
        const std::byte* address = nullptr;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access): v4l2_buffer.m é uma união do uAPI do kernel.
        if (const int rc = api_->mapBuffer(fd, buffer.length, buffer.m.offset, address); rc != 0) {
            return std::unexpected(stepError("mmap", rc));
        }
        session->adopt(address, buffer.length);
        if (const int rc = queueBuffer(*api_, fd, index); rc != 0) {
            return std::unexpected(stepError("VIDIOC_QBUF", rc));
        }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (const int rc = api_->ioctl(fd, requestStreamOn, &type); rc != 0) {
        return std::unexpected(stepError("VIDIOC_STREAMON", rc));
    }
    session->markStreaming();
    return std::unique_ptr<VideoCaptureSession>{std::move(session)};
}

} // namespace ghub::camera
