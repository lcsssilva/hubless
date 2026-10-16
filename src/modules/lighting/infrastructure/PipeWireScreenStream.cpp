#include "modules/lighting/infrastructure/PipeWireScreenStream.hpp"

#include "modules/lighting/domain/ScreenRegions.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <utility>

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>

namespace ghub::lighting::infrastructure {
namespace {

constexpr std::uint32_t kRegionColumns = 10;
constexpr std::uint32_t kSampleStep = 8; // um pixel a cada 8 em x e y: bastam para a cor média de uma região grande
constexpr std::chrono::milliseconds kMinFrameInterval{50}; // no máximo 20 quadros por segundo reduzidos

void initPipeWireOnce()
{
    static std::once_flag once;
    std::call_once(once, [] { pw_init(nullptr, nullptr); });
}

} // namespace

class PipeWireScreenStream::Impl {
public:
    explicit Impl(RegionsCallback onRegions) : onRegions_(std::move(onRegions)) {}
    ~Impl() { teardown(); }
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    shared_kernel::Result<void> start(ScreenFd fd, std::uint32_t nodeId, const std::string& remoteName)
    {
        initPipeWireOnce();
        loop_ = pw_thread_loop_new("ghub-screen-sampler", nullptr);
        if (loop_ == nullptr || pw_thread_loop_start(loop_) < 0) {
            teardown();
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                       "não foi possível iniciar o loop do PipeWire");
        }
        pw_thread_loop_lock(loop_);
        auto opened = openLocked(std::move(fd), nodeId, remoteName);
        pw_thread_loop_unlock(loop_);
        if (!opened.has_value()) {
            teardown();
        }
        return opened;
    }

private:
    shared_kernel::Result<void> openLocked(ScreenFd fd, std::uint32_t nodeId, const std::string& remoteName)
    {
        context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
        if (context_ == nullptr) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                       "não foi possível criar o contexto do PipeWire");
        }
        if (fd.valid()) {
            core_ = pw_context_connect_fd(context_, fd.release(), nullptr, 0); // o PipeWire passa a fechar o fd
        } else {
            pw_properties* remote =
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): pw_properties_new é variádica na API C
                remoteName.empty() ? nullptr : pw_properties_new(PW_KEY_REMOTE_NAME, remoteName.c_str(), nullptr);
            core_ = pw_context_connect(context_, remote, 0);
        }
        if (core_ == nullptr) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                       "não foi possível conectar ao PipeWire");
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): pw_properties_new é variádica na API C
        pw_properties* props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture",
                                                 PW_KEY_MEDIA_ROLE, "Screen", PW_KEY_NODE_NAME, "ghub.screen-sampler",
                                                 PW_KEY_NODE_DESCRIPTION, "Amostragem da tela para o teclado", nullptr);
        stream_ = pw_stream_new(core_, "ghub-screen-sampler", props);
        if (stream_ == nullptr) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                       "não foi possível criar o fluxo de vídeo");
        }
        pw_stream_add_listener(stream_, &listener_, &events, this);
        listening_ = true;

        std::array<std::uint8_t, 1024> buffer{};
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer.data(), static_cast<std::uint32_t>(buffer.size()));
        const spa_rectangle defaultSize{1920, 1080};
        const spa_rectangle minSize{1, 1};
        const spa_rectangle maxSize{16384, 16384};
        const spa_fraction defaultRate{30, 1};
        const spa_fraction minRate{0, 1};
        const spa_fraction maxRate{1000, 1};
        const spa_pod* params[1] = {static_cast<const spa_pod*>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat, SPA_FORMAT_mediaType,
            SPA_POD_Id(SPA_MEDIA_TYPE_video), SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format,
            SPA_POD_CHOICE_ENUM_Id(5, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx,
                                   SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBA),
            SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&defaultSize, &minSize, &maxSize),
            SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&defaultRate, &minRate, &maxRate)))};
        const auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);
        if (params[0] == nullptr ||
            pw_stream_connect(stream_, PW_DIRECTION_INPUT, nodeId == 0 ? PW_ID_ANY : nodeId, flags, params, 1) < 0) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                       "não foi possível conectar o fluxo de vídeo");
        }
        return {};
    }

    void teardown()
    {
        if (loop_ == nullptr) {
            return;
        }
        pw_thread_loop_lock(loop_);
        if (listening_) {
            spa_hook_remove(&listener_);
            listening_ = false;
        }
        if (stream_ != nullptr) {
            pw_stream_destroy(stream_);
            stream_ = nullptr;
        }
        if (core_ != nullptr) {
            pw_core_disconnect(core_);
            core_ = nullptr;
        }
        if (context_ != nullptr) {
            pw_context_destroy(context_);
            context_ = nullptr;
        }
        pw_thread_loop_unlock(loop_);
        pw_thread_loop_stop(loop_);
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
    }

    // Roda na thread do loop.
    void onParamChanged(std::uint32_t id, const spa_pod* param)
    {
        if (id != SPA_PARAM_Format || param == nullptr) {
            return;
        }
        spa_video_info_raw info{};
        if (spa_format_video_raw_parse(param, &info) < 0) {
            formatKnown_ = false;
            return;
        }
        formatKnown_ = true;
        width_ = info.size.width;
        height_ = info.size.height;
        const auto format = info.format;
        if (format == SPA_VIDEO_FORMAT_BGRx || format == SPA_VIDEO_FORMAT_BGRA) {
            pixelFormat_ = domain::PixelFormat::Bgrx;
        } else if (format == SPA_VIDEO_FORMAT_RGBx || format == SPA_VIDEO_FORMAT_RGBA) {
            pixelFormat_ = domain::PixelFormat::Rgbx;
        } else {
            formatKnown_ = false; // não negociado por nós: ignora os quadros
        }
    }

    void onStateChanged(pw_stream_state previous, pw_stream_state state)
    {
        const bool ended = state == PW_STREAM_STATE_ERROR ||
                           (state == PW_STREAM_STATE_UNCONNECTED && previous != PW_STREAM_STATE_UNCONNECTED);
        if (ended) {
            onRegions_({}); // o compartilhamento acabou: a fonte esquece as cores
        }
    }

    // Reduz o quadro a médias e devolve o buffer; nada do quadro é guardado.
    void onProcess()
    {
        pw_buffer* buffer = pw_stream_dequeue_buffer(stream_);
        if (buffer == nullptr) {
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        if (formatKnown_ && (!lastFrame_.has_value() || now - *lastFrame_ >= kMinFrameInterval)) {
            const spa_data& data = buffer->buffer->datas[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const bool corrupted = data.chunk != nullptr &&
                                   (static_cast<std::uint32_t>(data.chunk->flags) & SPA_CHUNK_FLAG_CORRUPTED) != 0;
            if (data.data != nullptr && data.chunk != nullptr && !corrupted && data.chunk->size > 0) {
                const auto offset = std::min<std::size_t>(data.chunk->offset, data.maxsize);
                const auto size = std::min<std::size_t>(data.chunk->size, data.maxsize - offset);
                const std::uint32_t stride =
                    data.chunk->stride > 0 ? static_cast<std::uint32_t>(data.chunk->stride) : width_ * 4;
                const std::span<const std::uint8_t> pixels{
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    static_cast<const std::uint8_t*>(data.data) + offset, size};
                auto regions = domain::averageRegions({pixels, width_, height_, stride, pixelFormat_},
                                                      {.columns = kRegionColumns, .rows = 1}, kSampleStep);
                if (regions.has_value()) {
                    lastFrame_ = now;
                    onRegions_(*std::move(regions));
                }
            }
        }
        pw_stream_queue_buffer(stream_, buffer);
    }

    static void paramChangedTrampoline(void* userdata, std::uint32_t id, const spa_pod* param)
    {
        static_cast<Impl*>(userdata)->onParamChanged(id, param);
    }
    static void stateChangedTrampoline(void* userdata, pw_stream_state previous, pw_stream_state state, const char*)
    {
        static_cast<Impl*>(userdata)->onStateChanged(previous, state);
    }
    static void processTrampoline(void* userdata) { static_cast<Impl*>(userdata)->onProcess(); }

    static pw_stream_events makeEvents()
    {
        pw_stream_events e{};
        e.version = PW_VERSION_STREAM_EVENTS;
        e.param_changed = &Impl::paramChangedTrampoline;
        e.state_changed = &Impl::stateChangedTrampoline;
        e.process = &Impl::processTrampoline;
        return e;
    }
    inline static const pw_stream_events events = makeEvents();

    RegionsCallback onRegions_;
    // Estado do formato e do ritmo: só a thread do loop toca.
    bool formatKnown_ = false;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    domain::PixelFormat pixelFormat_ = domain::PixelFormat::Bgrx;
    std::optional<std::chrono::steady_clock::time_point> lastFrame_;

    pw_thread_loop* loop_ = nullptr;
    pw_context* context_ = nullptr;
    pw_core* core_ = nullptr;
    pw_stream* stream_ = nullptr;
    spa_hook listener_{};
    bool listening_ = false;
};

PipeWireScreenStream::PipeWireScreenStream(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

PipeWireScreenStream::~PipeWireScreenStream() = default;

shared_kernel::Result<std::unique_ptr<ScreenStream>>
PipeWireScreenStream::open(ScreenFd fd, std::uint32_t nodeId, RegionsCallback onRegions, const std::string& remoteName)
{
    auto impl = std::make_unique<Impl>(std::move(onRegions));
    if (auto started = impl->start(std::move(fd), nodeId, remoteName); !started.has_value()) {
        return std::unexpected(started.error());
    }
    return std::unique_ptr<ScreenStream>{new PipeWireScreenStream{std::move(impl)}};
}

} // namespace ghub::lighting::infrastructure
