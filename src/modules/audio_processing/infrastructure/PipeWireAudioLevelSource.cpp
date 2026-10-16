#include "modules/audio_processing/infrastructure/PipeWireAudioLevelSource.hpp"

#include "modules/audio_processing/domain/BandMeter.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <utility>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>

namespace ghub::audio_processing {
namespace {

constexpr std::uint32_t kSampleRate = 48000;

void initPipeWireOnce()
{
    static std::once_flag once;
    std::call_once(once, [] { pw_init(nullptr, nullptr); });
}

} // namespace

// Estado do fluxo: `meter_` é escrito pelo callback (thread do loop) e lido por levels() (qualquer thread), sempre
// sob `mutex_`. Os objetos do PipeWire só são tocados com o lock do pw_thread_loop.
class PipeWireAudioLevelSource::Impl {
public:
    explicit Impl(std::string remoteName) : remoteName_(std::move(remoteName)), meter_(kSampleRate) {}

    ~Impl() { stop(); }
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    shared_kernel::Result<void> start()
    {
        if (running_) {
            return {};
        }
        initPipeWireOnce();
        loop_ = pw_thread_loop_new("ghub-audio-levels", nullptr);
        if (loop_ == nullptr || pw_thread_loop_start(loop_) < 0) {
            teardown();
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                       "não foi possível iniciar o loop do PipeWire");
        }
        pw_thread_loop_lock(loop_);
        auto opened = openLocked();
        pw_thread_loop_unlock(loop_);
        if (!opened.has_value()) {
            teardown();
            return opened;
        }
        running_ = true;
        return {};
    }

    void stop()
    {
        teardown();
        const std::scoped_lock lock{mutex_};
        meter_.clear();
    }

    [[nodiscard]] bool running() const { return running_; }

    [[nodiscard]] std::array<float, 10> levels() const
    {
        const std::scoped_lock lock{mutex_};
        return meter_.levels();
    }

private:
    shared_kernel::Result<void> openLocked()
    {
        context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
        if (context_ == nullptr) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                       "não foi possível criar o contexto do PipeWire");
        }
        pw_properties* remote =
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): pw_properties_new é variádica na API C
            remoteName_.empty() ? nullptr : pw_properties_new(PW_KEY_REMOTE_NAME, remoteName_.c_str(), nullptr);
        core_ = pw_context_connect(context_, remote, 0);
        if (core_ == nullptr) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                       "não foi possível conectar ao PipeWire");
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): pw_properties_new é variádica na API C
        pw_properties* props =
            pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "Music",
                              PW_KEY_NODE_NAME, "ghub.audio-visualizer", PW_KEY_NODE_DESCRIPTION,
                              "Visualizador de áudio do teclado", PW_KEY_STREAM_CAPTURE_SINK, "true", nullptr);
        stream_ = pw_stream_new(core_, "ghub-audio-visualizer", props);
        if (stream_ == nullptr) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                       "não foi possível criar o fluxo de captura");
        }
        pw_stream_add_listener(stream_, &listener_, &events, this);
        listening_ = true;

        std::array<std::uint8_t, 256> buffer{};
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer.data(), static_cast<std::uint32_t>(buffer.size()));
        std::array<std::uint32_t, 1> position{SPA_AUDIO_CHANNEL_MONO};
        spa_audio_info_raw format{};
        format.format = SPA_AUDIO_FORMAT_F32;
        format.rate = kSampleRate;
        format.channels = 1;
        format.position[0] = position[0];
        const spa_pod* params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &format)};
        const auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);
        if (pw_stream_connect(stream_, PW_DIRECTION_INPUT, PW_ID_ANY, flags, params, 1) < 0) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                       "não foi possível conectar o fluxo de captura");
        }
        return {};
    }

    void teardown()
    {
        running_ = false;
        if (loop_ != nullptr) {
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
    }

    // Roda na thread do loop. Copia as amostras para o medidor e devolve o buffer; nada mais é guardado.
    void onProcess()
    {
        pw_buffer* buffer = pw_stream_dequeue_buffer(stream_);
        if (buffer == nullptr) {
            return;
        }
        const spa_data& data = buffer->buffer->datas[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (data.data != nullptr && data.chunk != nullptr && data.chunk->size >= sizeof(float)) {
            const auto bytes = std::min<std::size_t>(data.chunk->size, data.maxsize);
            const auto offset = std::min<std::size_t>(data.chunk->offset, data.maxsize);
            const auto available = std::min(bytes, data.maxsize - offset);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const auto* samples = reinterpret_cast<const float*>(static_cast<const std::uint8_t*>(data.data) + offset);
            const std::scoped_lock lock{mutex_};
            meter_.push(std::span<const float>{samples, available / sizeof(float)});
        }
        pw_stream_queue_buffer(stream_, buffer);
    }

    static void processTrampoline(void* userdata) { static_cast<Impl*>(userdata)->onProcess(); }

    static pw_stream_events makeEvents()
    {
        pw_stream_events e{};
        e.version = PW_VERSION_STREAM_EVENTS;
        e.process = &Impl::processTrampoline;
        return e;
    }
    inline static const pw_stream_events events = makeEvents();

    const std::string remoteName_;
    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;
    BandMeter meter_;
    pw_thread_loop* loop_ = nullptr;
    pw_context* context_ = nullptr;
    pw_core* core_ = nullptr;
    pw_stream* stream_ = nullptr;
    spa_hook listener_{};
    bool listening_ = false;
};

PipeWireAudioLevelSource::PipeWireAudioLevelSource(std::string remoteName)
    : impl_(std::make_unique<Impl>(std::move(remoteName)))
{
}

PipeWireAudioLevelSource::~PipeWireAudioLevelSource() = default;

shared_kernel::Result<void> PipeWireAudioLevelSource::start()
{
    return impl_->start();
}

void PipeWireAudioLevelSource::stop()
{
    impl_->stop();
}

bool PipeWireAudioLevelSource::running() const
{
    return impl_->running();
}

std::array<float, 10> PipeWireAudioLevelSource::levels() const
{
    return impl_->levels();
}

} // namespace ghub::audio_processing
