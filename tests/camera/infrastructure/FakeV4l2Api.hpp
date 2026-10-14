#pragma once

#include "modules/camera/infrastructure/V4l2Api.hpp"

#include <linux/videodev2.h>
#include <poll.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ghub::camera::testing {

struct FakeInterval {
    std::uint32_t numerator; // segundos por quadro, como no V4L2 (1/30 s => {1, 30})
    std::uint32_t denominator;
};

struct FakeFrameSize {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<FakeInterval> intervals;
    bool stepwiseIntervals = false; // o driver devolve o primeiro intervalo como STEPWISE
};

struct FakeFormat {
    std::string fourcc; // 4 caracteres; ignorado se rawPixelFormat estiver preenchido
    std::vector<FakeFrameSize> sizes;
    bool stepwiseSizes = false; // o driver devolve o primeiro tamanho como STEPWISE
    std::optional<std::uint32_t> rawPixelFormat;
};

struct FakeMenuItem {
    std::uint32_t index;
    std::string name;
};

struct FakeControl {
    std::uint32_t id;
    std::uint32_t type; // V4L2_CTRL_TYPE_*
    std::string name;
    std::int64_t minimum = 0;
    std::int64_t maximum = 0;
    std::uint64_t step = 1;
    std::int64_t defaultValue = 0;
    std::int32_t value = 0;
    std::uint32_t flags = 0; // V4L2_CTRL_FLAG_*
    std::vector<FakeMenuItem> items;
};

struct FakeNode {
    std::string path;
    std::string card;
    std::uint32_t capabilities = 0;
    std::uint32_t deviceCaps = 0;
    std::vector<FakeFormat> formats;
    std::vector<FakeControl> controls;
};

// Quadro roteirizado pelo teste para o próximo DQBUF (streaming).
struct FakeFrame {
    std::vector<std::byte> bytes;           // conteúdo escrito no buffer
    std::uint32_t flags = 0;                // V4L2_BUF_FLAG_*
    std::optional<std::uint32_t> bytesUsed; // força `bytesused` (padrão: bytes.size())
    std::optional<std::uint32_t> index;     // força o índice devolvido (padrão: próximo buffer enfileirado)
};

// Lado do mapeamento registrado por mapBuffer.
struct FakeMapping {
    const std::byte* address;
    std::size_t length;
    std::uint32_t offset;
};

// API V4L2 falsa: responde aos ioctls de consulta como um driver UVC (EINVAL no fim das listas,
// NEXT_CTRL, EINVAL em item de menu inexistente). Registra descritores e requests; request
// desconhecido devolve -ENOTTY e fica registrado.
class FakeV4l2Api final : public V4l2Api {
public:
    // --- configuração ---
    std::vector<FakeNode> nodes;
    std::map<unsigned long, int> requestErrors;      // request -> -errno (vale para todos os usos)
    std::map<std::string, int> openErrors;           // caminho -> -errno
    std::map<std::uint32_t, int> controlValueErrors; // id do controle -> -errno em G_CTRL
    bool endlessFormats = false;                     // ENUM_FMT nunca devolve EINVAL
    bool stuckControlIds = false;                    // QUERY_EXT_CTRL devolve sempre o primeiro controle
    bool skipDeviceCapsFlag = false;                 // omite V4L2_CAP_DEVICE_CAPS (usa só `capabilities`)

    // --- configuração do streaming (driver UVC simulado) ---
    std::map<std::string, int> streamingOpenErrors; // caminho -> -errno só em openForStreaming
    bool queueBusy = false;                         // outro programa é dono da fila: S_FMT/REQBUFS/STREAMON → -EBUSY
    std::optional<std::uint32_t> grantedBuffers;    // REQBUFS concede isto (padrão: o pedido)
    std::uint32_t bufferLength = 65536;             // tamanho real da memória de cada buffer
    std::map<std::uint32_t, std::uint32_t> reportedLengths; // índice -> `length` informado por QUERYBUF
    std::map<std::uint32_t, std::uint32_t> reportedIndexes; // índice -> `index` informado por QUERYBUF
    std::map<std::size_t, int> mapErrors;                   // n-ésima chamada de mapBuffer (de 0) -> -errno
    std::map<std::size_t, int> queueBufferErrors;           // n-ésimo VIDIOC_QBUF (de 0) -> -errno
    std::deque<FakeFrame> frames;                           // quadros que os próximos DQBUF entregam
    std::deque<int> pollResults;                            // poll roteirizado (revents ou -errno), consumido em ordem
    // Campos que o S_FMT devolve forçados, depois do ajuste normal (permite alterar um único campo do formato).
    std::optional<std::uint32_t> forcedReplyPixelFormat;
    std::optional<std::uint32_t> forcedReplyWidth;
    std::optional<std::uint32_t> forcedReplyHeight;

    // --- registro ---
    std::vector<unsigned long> requests;
    // Valores pedidos em cada VIDIOC_S_CTRL (como chegaram ao "driver", antes de arredondar/limitar).
    struct ControlWrite {
        std::uint32_t id;
        std::int32_t requested;
        std::int32_t applied;
    };
    std::vector<ControlWrite> controlWrites;
    std::vector<int> opened;
    std::vector<int> closed;
    std::vector<int> streamingOpened;         // descritores abertos por openForStreaming
    std::optional<v4l2_format> lastSetFormat; // formato pedido no último S_FMT
    std::vector<FakeMapping> mappings;
    std::vector<const std::byte*> unmapped;
    std::vector<std::chrono::milliseconds> pollTimeouts;
    std::vector<std::uint32_t> queuedIndexes; // índice de cada QBUF bem-sucedido, em ordem
    bool streaming = false;
    int streamOnCalls = 0;
    int streamOffCalls = 0;

    [[nodiscard]] int open(const std::filesystem::path& node) override
    {
        const std::string path = node.string();
        if (const auto error = openErrors.find(path); error != openErrors.end()) {
            return error->second;
        }
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            if (nodes[index].path == path) {
                const int fd = nextFd_++;
                fdToNode_[fd] = index;
                opened.push_back(fd);
                return fd;
            }
        }
        return -ENOENT;
    }

    [[nodiscard]] int openForStreaming(const std::filesystem::path& node) override
    {
        if (const auto error = streamingOpenErrors.find(node.string()); error != streamingOpenErrors.end()) {
            return error->second;
        }
        const int fd = open(node);
        if (fd >= 0) {
            streamingOpened.push_back(fd);
        }
        return fd;
    }

    [[nodiscard]] int mapBuffer(int fd, std::size_t length, std::uint32_t offset, const std::byte*& address) override
    {
        const std::size_t call = mapCalls_++;
        if (!isOpen(fd)) {
            return -EBADF;
        }
        if (const auto error = mapErrors.find(call); error != mapErrors.end()) {
            return error->second;
        }
        const std::size_t index = offset / bufferOffsetStep;
        if (index >= buffers_.size()) {
            return -EINVAL;
        }
        address = buffers_[index].data();
        mappings.push_back({.address = address, .length = length, .offset = offset});
        return 0;
    }

    void unmapBuffer(const std::byte* address, std::size_t /*length*/) noexcept override
    {
        unmapped.push_back(address);
    }

    [[nodiscard]] int poll(int fd, std::chrono::milliseconds timeout) override
    {
        pollTimeouts.push_back(timeout);
        if (!isOpen(fd)) {
            return -EBADF;
        }
        if (!pollResults.empty()) {
            const int result = pollResults.front();
            pollResults.pop_front();
            return result;
        }
        return streaming && !frames.empty() ? POLLIN : 0;
    }

    [[nodiscard]] int ioctl(int fd, unsigned long request, void* argument) override
    {
        requests.push_back(request);
        const auto it = fdToNode_.find(fd);
        if (it == fdToNode_.end() || std::ranges::contains(closed, fd)) {
            return -EBADF;
        }
        if (const auto error = requestErrors.find(request); error != requestErrors.end()) {
            return error->second;
        }
        const FakeNode& node = nodes[it->second];
        if (request == VIDIOC_QUERYCAP) {
            return queryCap(node, *static_cast<v4l2_capability*>(argument));
        }
        if (request == VIDIOC_ENUM_FMT) {
            return enumFmt(node, *static_cast<v4l2_fmtdesc*>(argument));
        }
        if (request == VIDIOC_ENUM_FRAMESIZES) {
            return enumFrameSizes(node, *static_cast<v4l2_frmsizeenum*>(argument));
        }
        if (request == VIDIOC_ENUM_FRAMEINTERVALS) {
            return enumFrameIntervals(node, *static_cast<v4l2_frmivalenum*>(argument));
        }
        if (request == VIDIOC_QUERY_EXT_CTRL) {
            return queryExtCtrl(node, *static_cast<v4l2_query_ext_ctrl*>(argument));
        }
        if (request == VIDIOC_QUERYMENU) {
            return queryMenu(node, *static_cast<v4l2_querymenu*>(argument));
        }
        if (request == VIDIOC_G_CTRL) {
            return getCtrl(node, *static_cast<v4l2_control*>(argument));
        }
        if (request == VIDIOC_S_CTRL) {
            return setCtrl(it->second, *static_cast<v4l2_control*>(argument));
        }
        if (request == VIDIOC_S_FMT) {
            return setFormat(node, *static_cast<v4l2_format*>(argument));
        }
        if (request == VIDIOC_REQBUFS) {
            return requestBuffers(*static_cast<v4l2_requestbuffers*>(argument));
        }
        if (request == VIDIOC_QUERYBUF) {
            return queryBuffer(*static_cast<v4l2_buffer*>(argument));
        }
        if (request == VIDIOC_QBUF) {
            return queueBuffer(*static_cast<v4l2_buffer*>(argument));
        }
        if (request == VIDIOC_DQBUF) {
            return dequeueBuffer(*static_cast<v4l2_buffer*>(argument));
        }
        if (request == VIDIOC_STREAMON) {
            ++streamOnCalls;
            if (queueBusy) {
                return -EBUSY;
            }
            streaming = true;
            return 0;
        }
        if (request == VIDIOC_STREAMOFF) {
            ++streamOffCalls;
            streaming = false;
            return 0;
        }
        return -ENOTTY;
    }

    void close(int fd) noexcept override { closed.push_back(fd); }

    // Caminho do nó que `fd` abriu (vazio se o descritor não existe).
    [[nodiscard]] std::string pathOf(int fd) const
    {
        const auto it = fdToNode_.find(fd);
        return it == fdToNode_.end() ? std::string{} : nodes[it->second].path;
    }

    [[nodiscard]] bool allDescriptorsClosed() const
    {
        auto a = opened;
        auto b = closed;
        std::ranges::sort(a);
        std::ranges::sort(b);
        return a == b;
    }

private:
    // Os buffers de captura ficam em offsets múltiplos deste passo (como os offsets de mmap do vb2).
    static constexpr std::uint32_t bufferOffsetStep = 0x10000;

    [[nodiscard]] bool isOpen(int fd) const { return fdToNode_.contains(fd) && !std::ranges::contains(closed, fd); }

    // O driver aceita o modo se existir; senão ajusta para o primeiro formato/tamanho, sem erro (uvcvideo).
    int setFormat(const FakeNode& node, v4l2_format& format)
    {
        if (format.type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            return -EINVAL;
        }
        lastSetFormat = format;
        if (queueBusy) {
            return -EBUSY;
        }
        // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access): v4l2_format é uma união do uAPI do kernel;
        // `pix` é o membro válido para V4L2_BUF_TYPE_VIDEO_CAPTURE (verificado acima).
        const FakeFormat* wanted = findFormat(node, format.fmt.pix.pixelformat);
        const bool sizeExists =
            wanted != nullptr && std::ranges::any_of(wanted->sizes, [&format](const FakeFrameSize& size) {
                return size.width == format.fmt.pix.width && size.height == format.fmt.pix.height;
            });
        if (!sizeExists) {
            if (node.formats.empty() || node.formats.front().sizes.empty()) {
                return -EINVAL;
            }
            format.fmt.pix.pixelformat = rawOf(node.formats.front());
            format.fmt.pix.width = node.formats.front().sizes.front().width;
            format.fmt.pix.height = node.formats.front().sizes.front().height;
        }
        format.fmt.pix.pixelformat = forcedReplyPixelFormat.value_or(format.fmt.pix.pixelformat);
        format.fmt.pix.width = forcedReplyWidth.value_or(format.fmt.pix.width);
        format.fmt.pix.height = forcedReplyHeight.value_or(format.fmt.pix.height);
        format.fmt.pix.field = V4L2_FIELD_NONE;
        format.fmt.pix.sizeimage = format.fmt.pix.width * format.fmt.pix.height * 2;
        // NOLINTEND(cppcoreguidelines-pro-type-union-access)
        return 0;
    }

    int requestBuffers(v4l2_requestbuffers& request)
    {
        if (queueBusy) {
            return -EBUSY;
        }
        if (request.type != V4L2_BUF_TYPE_VIDEO_CAPTURE || request.memory != V4L2_MEMORY_MMAP) {
            return -EINVAL;
        }
        request.count = grantedBuffers.value_or(request.count);
        buffers_.assign(request.count, std::vector<std::byte>(bufferLength));
        pending_.clear();
        return 0;
    }

    int queryBuffer(v4l2_buffer& buffer) const
    {
        if (buffer.index >= buffers_.size()) {
            return -EINVAL;
        }
        const std::uint32_t index = buffer.index;
        buffer.length = bufferLength;
        if (const auto length = reportedLengths.find(index); length != reportedLengths.end()) {
            buffer.length = length->second;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access): v4l2_buffer.m é uma união do uAPI do kernel.
        buffer.m.offset = index * bufferOffsetStep;
        if (const auto reported = reportedIndexes.find(index); reported != reportedIndexes.end()) {
            buffer.index = reported->second;
        }
        return 0;
    }

    int queueBuffer(const v4l2_buffer& buffer)
    {
        const std::size_t call = queueBufferCalls_++;
        if (const auto error = queueBufferErrors.find(call); error != queueBufferErrors.end()) {
            return error->second;
        }
        if (buffer.index >= buffers_.size()) {
            return -EINVAL;
        }
        pending_.push_back(buffer.index);
        queuedIndexes.push_back(buffer.index);
        return 0;
    }

    int dequeueBuffer(v4l2_buffer& buffer)
    {
        if (frames.empty() || !streaming) {
            return -EAGAIN;
        }
        FakeFrame frame = std::move(frames.front());
        frames.pop_front();
        std::uint32_t index = 0;
        if (frame.index.has_value()) {
            index = *frame.index;
        } else if (pending_.empty()) {
            frames.push_front(std::move(frame));
            return -EAGAIN; // o driver não tem buffer livre para preencher
        } else {
            index = pending_.front();
            pending_.pop_front();
        }
        if (index < buffers_.size()) {
            std::ranges::copy_n(frame.bytes.begin(),
                                static_cast<std::ptrdiff_t>(std::min<std::size_t>(frame.bytes.size(), bufferLength)),
                                buffers_[index].begin());
        }
        buffer.index = index;
        buffer.bytesused = frame.bytesUsed.value_or(static_cast<std::uint32_t>(frame.bytes.size()));
        buffer.flags = frame.flags;
        return 0;
    }

    static std::uint32_t fourccToRaw(const std::string& fourcc)
    {
        std::uint32_t raw = 0;
        for (std::size_t i = 0; i < 4 && i < fourcc.size(); ++i) {
            raw |= static_cast<std::uint32_t>(static_cast<unsigned char>(fourcc[i])) << (8U * i);
        }
        return raw;
    }

    static std::uint32_t rawOf(const FakeFormat& format)
    {
        return format.rawPixelFormat.value_or(fourccToRaw(format.fourcc));
    }

    static const FakeFormat* findFormat(const FakeNode& node, std::uint32_t raw)
    {
        const auto found = std::ranges::find_if(node.formats, [raw](const FakeFormat& f) { return rawOf(f) == raw; });
        return found == node.formats.end() ? nullptr : &*found;
    }

    // Copia `text` para um buffer fixo do kernel (sem NUL final se o texto ocupar o buffer inteiro).
    template <typename Char> static void copyText(std::span<Char> destination, const std::string& text)
    {
        std::ranges::fill(destination, Char{});
        std::ranges::transform(text | std::views::take(destination.size()), destination.begin(),
                               [](char c) { return static_cast<Char>(c); });
    }

    int queryCap(const FakeNode& node, v4l2_capability& cap) const
    {
        cap = {};
        copyText(std::span<std::uint8_t>{cap.card}, node.card);
        cap.capabilities = node.capabilities;
        cap.device_caps = node.deviceCaps;
        if (!skipDeviceCapsFlag) {
            cap.capabilities |= V4L2_CAP_DEVICE_CAPS;
        }
        return 0;
    }

    int enumFmt(const FakeNode& node, v4l2_fmtdesc& desc) const
    {
        if (desc.type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            return -EINVAL;
        }
        if (endlessFormats) {
            desc.pixelformat = fourccToRaw("MJPG");
            return 0;
        }
        if (desc.index >= node.formats.size()) {
            return -EINVAL;
        }
        desc.pixelformat = rawOf(node.formats[desc.index]);
        return 0;
    }

    static int enumFrameSizes(const FakeNode& node, v4l2_frmsizeenum& size)
    {
        const FakeFormat* format = findFormat(node, size.pixel_format);
        if (format == nullptr || size.index >= format->sizes.size()) {
            return -EINVAL;
        }
        const FakeFrameSize& entry = format->sizes[size.index];
        if (format->stepwiseSizes) {
            size.type = V4L2_FRMSIZE_TYPE_STEPWISE;
            return 0;
        }
        size.type = V4L2_FRMSIZE_TYPE_DISCRETE;
        // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access): v4l2_frmsizeenum é uma união do uAPI do kernel.
        size.discrete.width = entry.width;
        size.discrete.height = entry.height;
        // NOLINTEND(cppcoreguidelines-pro-type-union-access)
        return 0;
    }

    static int enumFrameIntervals(const FakeNode& node, v4l2_frmivalenum& interval)
    {
        const FakeFormat* format = findFormat(node, interval.pixel_format);
        if (format == nullptr) {
            return -EINVAL;
        }
        const auto size = std::ranges::find_if(format->sizes, [&interval](const FakeFrameSize& s) {
            return s.width == interval.width && s.height == interval.height;
        });
        if (size == format->sizes.end() || interval.index >= size->intervals.size()) {
            return -EINVAL;
        }
        if (size->stepwiseIntervals) {
            interval.type = V4L2_FRMIVAL_TYPE_STEPWISE;
            return 0;
        }
        interval.type = V4L2_FRMIVAL_TYPE_DISCRETE;
        // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access): v4l2_frmivalenum é uma união do uAPI do kernel.
        interval.discrete.numerator = size->intervals.at(interval.index).numerator;
        interval.discrete.denominator = size->intervals.at(interval.index).denominator;
        // NOLINTEND(cppcoreguidelines-pro-type-union-access)
        return 0;
    }

    int queryExtCtrl(const FakeNode& node, v4l2_query_ext_ctrl& query) const
    {
        const FakeControl* found = nullptr;
        if (stuckControlIds) {
            found = node.controls.empty() ? nullptr : &node.controls.front();
        } else if ((query.id & V4L2_CTRL_FLAG_NEXT_CTRL) != 0) {
            const std::uint32_t after = query.id & ~V4L2_CTRL_FLAG_NEXT_CTRL;
            for (const FakeControl& control : node.controls) {
                if (control.id > after && (found == nullptr || control.id < found->id)) {
                    found = &control;
                }
            }
        } else {
            const auto exact = std::ranges::find(node.controls, query.id, &FakeControl::id);
            found = exact == node.controls.end() ? nullptr : &*exact;
        }
        if (found == nullptr) {
            return -EINVAL;
        }
        query = {};
        query.id = found->id;
        query.type = found->type;
        copyText(std::span<char>{query.name}, found->name);
        query.minimum = found->minimum;
        query.maximum = found->maximum;
        query.step = found->step;
        query.default_value = found->defaultValue;
        query.flags = found->flags;
        return 0;
    }

    static int queryMenu(const FakeNode& node, v4l2_querymenu& menu)
    {
        const auto control = std::ranges::find(node.controls, menu.id, &FakeControl::id);
        if (control == node.controls.end()) {
            return -EINVAL;
        }
        const auto item = std::ranges::find(control->items, menu.index, &FakeMenuItem::index);
        if (item == control->items.end()) {
            return -EINVAL;
        }
        // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access,cppcoreguidelines-pro-bounds-array-to-pointer-decay):
        // v4l2_querymenu é uma estrutura packed com união no uAPI do kernel; não dá para ligar span/referência ao
        // campo, então a cópia usa ponteiros.
        std::memset(menu.name, 0, sizeof(menu.name));
        std::memcpy(menu.name, item->name.data(), std::min(item->name.size(), sizeof(menu.name)));
        // NOLINTEND(cppcoreguidelines-pro-type-union-access,cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        return 0;
    }

    int getCtrl(const FakeNode& node, v4l2_control& control) const
    {
        if (const auto error = controlValueErrors.find(control.id); error != controlValueErrors.end()) {
            return error->second;
        }
        const auto found = std::ranges::find(node.controls, control.id, &FakeControl::id);
        if (found == node.controls.end()) {
            return -EINVAL;
        }
        control.value = found->value;
        return 0;
    }

    // Como o uvcvideo: inativo ou somente leitura -> EACCES; inteiro é arredondado ao passo e limitado à faixa em
    // silêncio; booleano é limitado a 0..1; menu fora da faixa -> ERANGE e item inexistente -> EINVAL.
    int setCtrl(std::size_t nodeIndex, v4l2_control& control)
    {
        auto& controls = nodes[nodeIndex].controls;
        const auto found = std::ranges::find(controls, control.id, &FakeControl::id);
        if (found == controls.end()) {
            return -EINVAL;
        }
        if ((found->flags & V4L2_CTRL_FLAG_INACTIVE) != 0 || (found->flags & V4L2_CTRL_FLAG_READ_ONLY) != 0) {
            return -EACCES;
        }
        const std::int32_t requested = control.value;
        std::int64_t applied = requested;
        if (found->type == V4L2_CTRL_TYPE_BOOLEAN) {
            applied = std::clamp<std::int64_t>(applied, 0, 1);
        } else if (found->type == V4L2_CTRL_TYPE_MENU) {
            if (applied < found->minimum || applied > found->maximum) {
                return -ERANGE;
            }
            const auto item = std::ranges::find_if(found->items, [applied](const FakeMenuItem& candidate) {
                return std::cmp_equal(candidate.index, applied);
            });
            if (item == found->items.end()) {
                return -EINVAL;
            }
        } else {
            const auto step = static_cast<std::int64_t>(found->step);
            const std::int64_t steps = (applied - found->minimum + (step / 2)) / step;
            applied = found->minimum + (steps * step);
            applied = std::clamp(applied, found->minimum, found->maximum);
        }
        found->value = static_cast<std::int32_t>(applied);
        controlWrites.push_back({.id = control.id, .requested = requested, .applied = found->value});
        return 0;
    }

    int nextFd_ = 100;
    std::map<int, std::size_t> fdToNode_;
    std::vector<std::vector<std::byte>> buffers_;
    std::deque<std::uint32_t> pending_; // buffers enfileirados, na ordem em que o driver os preenche
    std::size_t mapCalls_ = 0;
    std::size_t queueBufferCalls_ = 0;
};

namespace fixtures {

// Tabela lida da Brio 100 real em 2026-10-13 (teste físico + v4l2-ctl; ver
// docs/device-support/evidence/2026-10-13-brio100-leitura-produto-inc005.md), na ordem de enumeração do driver.
inline std::vector<std::pair<std::uint32_t, std::uint32_t>> brioSizes()
{
    return {
        {640, 480}, {160, 120},  {176, 144},  {320, 176},  {320, 240},   {352, 288}, {432, 240},
        {544, 288}, {640, 360},  {752, 416},  {800, 448},  {800, 600},   {864, 480}, {960, 544},
        {960, 720}, {1024, 576}, {1184, 656}, {1280, 720}, {1920, 1080},
    };
}

inline FakeControl control(std::uint32_t id, std::uint32_t type, std::string name, std::int64_t min, std::int64_t max,
                           std::int64_t def, std::int32_t value, std::uint32_t flags = 0)
{
    return FakeControl{
        .id = id,
        .type = type,
        .name = std::move(name),
        .minimum = min,
        .maximum = max,
        .step = 1,
        .defaultValue = def,
        .value = value,
        .flags = flags,
        .items = {},
    };
}

inline std::vector<FakeControl> brioControls()
{
    std::vector<FakeControl> controls;
    controls.push_back(control(V4L2_CID_USER_CLASS, V4L2_CTRL_TYPE_CTRL_CLASS, "User Controls", 0, 0, 0, 0,
                               V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_WRITE_ONLY));
    controls.push_back(control(V4L2_CID_BRIGHTNESS, V4L2_CTRL_TYPE_INTEGER, "Brightness", 0, 255, 128, 128));
    controls.push_back(control(V4L2_CID_CONTRAST, V4L2_CTRL_TYPE_INTEGER, "Contrast", 0, 255, 128, 128));
    controls.push_back(control(V4L2_CID_SATURATION, V4L2_CTRL_TYPE_INTEGER, "Saturation", 0, 255, 128, 128));
    controls.push_back(
        control(V4L2_CID_AUTO_WHITE_BALANCE, V4L2_CTRL_TYPE_BOOLEAN, "White Balance, Automatic", 0, 1, 1, 1));
    controls.push_back(control(V4L2_CID_GAIN, V4L2_CTRL_TYPE_INTEGER, "Gain", 0, 255, 0, 0));
    auto powerLine = control(V4L2_CID_POWER_LINE_FREQUENCY, V4L2_CTRL_TYPE_MENU, "Power Line Frequency", 0, 2, 2, 2);
    powerLine.items = {{.index = 0, .name = "Disabled"}, {.index = 1, .name = "50 Hz"}, {.index = 2, .name = "60 Hz"}};
    controls.push_back(powerLine);
    controls.push_back(control(V4L2_CID_WHITE_BALANCE_TEMPERATURE, V4L2_CTRL_TYPE_INTEGER, "White Balance Temperature",
                               2800, 7500, 4000, 4000, V4L2_CTRL_FLAG_INACTIVE));
    controls.push_back(control(V4L2_CID_SHARPNESS, V4L2_CTRL_TYPE_INTEGER, "Sharpness", 0, 255, 128, 128));
    controls.push_back(
        control(V4L2_CID_BACKLIGHT_COMPENSATION, V4L2_CTRL_TYPE_INTEGER, "Backlight Compensation", 0, 1, 1, 1));
    auto autoExposure = control(V4L2_CID_EXPOSURE_AUTO, V4L2_CTRL_TYPE_MENU, "Auto Exposure", 0, 3, 3, 3);
    autoExposure.items = {{.index = 1, .name = "Manual Mode"}, {.index = 3, .name = "Aperture Priority Mode"}};
    controls.push_back(autoExposure);
    controls.push_back(control(V4L2_CID_EXPOSURE_ABSOLUTE, V4L2_CTRL_TYPE_INTEGER, "Exposure Time, Absolute", 5, 2500,
                               156, 156, V4L2_CTRL_FLAG_INACTIVE));
    controls.push_back(
        control(V4L2_CID_EXPOSURE_AUTO_PRIORITY, V4L2_CTRL_TYPE_BOOLEAN, "Exposure, Dynamic Framerate", 0, 1, 0, 1));
    return controls;
}

inline FakeNode captureNode(std::string path, std::string card)
{
    FakeNode node;
    node.path = std::move(path);
    node.card = std::move(card);
    node.capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_META_CAPTURE | V4L2_CAP_STREAMING;
    node.deviceCaps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
    return node;
}

inline FakeNode metadataNode(std::string path, std::string card)
{
    FakeNode node;
    node.path = std::move(path);
    node.card = std::move(card);
    node.capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_META_CAPTURE | V4L2_CAP_STREAMING;
    node.deviceCaps = V4L2_CAP_META_CAPTURE | V4L2_CAP_STREAMING;
    return node;
}

// Brio 100 como o driver uvcvideo a enumera (evidência de 2026-10-13): captura + metadados.
inline FakeV4l2Api brio100(std::string capturePath = "/dev/video0", std::string metadataPath = "/dev/video1")
{
    const std::vector<FakeInterval> all{
        {.numerator = 1, .denominator = 30}, {.numerator = 1, .denominator = 24}, {.numerator = 1, .denominator = 20},
        {.numerator = 1, .denominator = 15}, {.numerator = 1, .denominator = 10}, {.numerator = 2, .denominator = 15},
        {.numerator = 1, .denominator = 5},
    };
    const std::vector<FakeInterval> slow{{.numerator = 1, .denominator = 5}};
    FakeFormat yuyv{.fourcc = "YUYV", .sizes = {}, .stepwiseSizes = false, .rawPixelFormat = std::nullopt};
    FakeFormat mjpg{.fourcc = "MJPG", .sizes = {}, .stepwiseSizes = false, .rawPixelFormat = std::nullopt};
    std::size_t position = 0;
    for (const auto& [width, height] : brioSizes()) {
        mjpg.sizes.push_back({.width = width, .height = height, .intervals = all, .stepwiseIntervals = false});
        // YUYV: os 9 primeiros tamanhos (até 640x360) têm os 7 intervalos; os demais só 1/5 s.
        yuyv.sizes.push_back(
            {.width = width, .height = height, .intervals = position < 9 ? all : slow, .stepwiseIntervals = false});
        ++position;
    }
    FakeNode capture = captureNode(std::move(capturePath), "Brio 100");
    capture.formats = {yuyv, mjpg};
    capture.controls = brioControls();
    FakeV4l2Api api;
    api.nodes.push_back(std::move(capture));
    api.nodes.push_back(metadataNode(std::move(metadataPath), "Brio 100"));
    return api;
}

} // namespace fixtures

} // namespace ghub::camera::testing
