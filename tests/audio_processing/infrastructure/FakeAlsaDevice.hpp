#pragma once

// Cliente de teste que exporta, para o daemon PRIVADO, um Device falso (props completas no evento `info`, como um
// dispositivo ALSA real) e um nó Audio/Sink apontando para ele por `device.id` (id global). Nunca usar fora do
// daemon privado.

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <pipewire/pipewire.h>
#include <spa/monitor/device.h>
#include <spa/monitor/utils.h>
#include <spa/utils/hook.h>

namespace ghub::audio_processing::testing {

class FakeAlsaDevice {
public:
    FakeAlsaDevice(const std::string& socketPath, const std::string& deviceName, const std::string& vendorId,
                   const std::string& productId, const std::string& sinkName,
                   const std::string& mediaClass = "Audio/Sink")
        : deviceName_(deviceName), vendorId_(vendorId), productId_(productId)
    {
        pw_init(nullptr, nullptr);
        loop_ = pw_thread_loop_new("fake-alsa", nullptr);
        pw_thread_loop_start(loop_);
        pw_thread_loop_lock(loop_);
        context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
        core_ = pw_context_connect(context_, pw_properties_new(PW_KEY_REMOTE_NAME, socketPath.c_str(), nullptr), 0);
        if (core_ == nullptr) {
            pw_thread_loop_unlock(loop_);
            throw std::runtime_error("FakeAlsaDevice: não conectou ao daemon privado");
        }
        spa_hook_list_init(&hooks_);
        device_.iface = SPA_INTERFACE_INIT(SPA_TYPE_INTERFACE_Device, SPA_VERSION_DEVICE, &deviceMethods, this);
        itemsStorage_[0] = SPA_DICT_ITEM_INIT(PW_KEY_MEDIA_CLASS, "Audio/Device");
        itemsStorage_[1] = SPA_DICT_ITEM_INIT(PW_KEY_DEVICE_API, "alsa");
        itemsStorage_[2] = SPA_DICT_ITEM_INIT(PW_KEY_DEVICE_NAME, deviceName_.c_str());
        itemsStorage_[3] = SPA_DICT_ITEM_INIT("device.vendor.id", vendorId_.c_str());
        itemsStorage_[4] = SPA_DICT_ITEM_INIT("device.product.id", productId_.c_str());
        infoProps_ = SPA_DICT_INIT(itemsStorage_, 5);
        const spa_dict_item exportItems[] = {SPA_DICT_ITEM_INIT(PW_KEY_DEVICE_NAME, deviceName_.c_str())};
        const spa_dict exportProps = SPA_DICT_INIT_ARRAY(exportItems);
        deviceProxy_ = pw_core_export(core_, SPA_TYPE_INTERFACE_Device, &exportProps, &device_, 0);
        if (deviceProxy_ == nullptr) {
            pw_thread_loop_unlock(loop_);
            throw std::runtime_error("FakeAlsaDevice: pw_core_export falhou");
        }
        pw_proxy_add_listener(deviceProxy_, &proxyListener_, &deviceProxyEvents, this);
        if (!waitFor([this] { return deviceGlobalId_ != 0; })) {
            pw_thread_loop_unlock(loop_);
            throw std::runtime_error("FakeAlsaDevice: o device não recebeu id global");
        }

        const std::string deviceId = std::to_string(deviceGlobalId_);
        pw_properties* nodeProps = pw_properties_new(
            "factory.name", "support.null-audio-sink", PW_KEY_NODE_NAME, sinkName.c_str(), PW_KEY_MEDIA_CLASS,
            mediaClass.c_str(), PW_KEY_DEVICE_ID, deviceId.c_str(), "audio.position", "[ FL FR ]", nullptr);
        nodeProxy_ = static_cast<pw_proxy*>(
            pw_core_create_object(core_, "adapter", PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, &nodeProps->dict, 0));
        pw_properties_free(nodeProps);
        if (nodeProxy_ == nullptr) {
            pw_thread_loop_unlock(loop_);
            throw std::runtime_error("FakeAlsaDevice: não criou o nó Audio/Sink");
        }
        pw_proxy_add_listener(nodeProxy_, &nodeListener_, &nodeProxyEvents, this);
        if (!waitFor([this] { return nodeBound_; })) {
            pw_thread_loop_unlock(loop_);
            throw std::runtime_error("FakeAlsaDevice: o nó não recebeu id global");
        }
        pw_thread_loop_unlock(loop_);
    }

    ~FakeAlsaDevice()
    {
        pw_thread_loop_lock(loop_);
        spa_hook_remove(&proxyListener_);
        spa_hook_remove(&nodeListener_);
        if (nodeProxy_ != nullptr) {
            pw_proxy_destroy(nodeProxy_);
        }
        if (deviceProxy_ != nullptr) {
            pw_proxy_destroy(deviceProxy_);
        }
        pw_core_disconnect(core_);
        pw_context_destroy(context_);
        pw_thread_loop_unlock(loop_);
        pw_thread_loop_stop(loop_);
        pw_thread_loop_destroy(loop_);
    }

    FakeAlsaDevice(const FakeAlsaDevice&) = delete;
    FakeAlsaDevice& operator=(const FakeAlsaDevice&) = delete;
    FakeAlsaDevice(FakeAlsaDevice&&) = delete;
    FakeAlsaDevice& operator=(FakeAlsaDevice&&) = delete;

    [[nodiscard]] std::uint32_t deviceGlobalId() const noexcept { return deviceGlobalId_; }

private:
    template <typename Predicate> bool waitFor(Predicate done)
    {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (!done()) {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - std::chrono::steady_clock::now());
            if (remaining.count() <= 0) {
                return false;
            }
            timespec abstime{};
            pw_thread_loop_get_time(loop_, &abstime, remaining.count());
            pw_thread_loop_timed_wait_full(loop_, &abstime);
        }
        return true;
    }

    static int addListener(void* object, spa_hook* listener, const spa_device_events* events, void* data)
    {
        auto* self = static_cast<FakeAlsaDevice*>(object);
        spa_hook_list save;
        spa_hook_list_isolate(&self->hooks_, &save, listener, events, data);
        spa_device_info info = SPA_DEVICE_INFO_INIT();
        info.change_mask = SPA_DEVICE_CHANGE_MASK_PROPS;
        info.props = &self->infoProps_;
        spa_device_emit_info(&self->hooks_, &info);
        spa_hook_list_join(&self->hooks_, &save);
        return 0;
    }
    static int sync(void*, int) { return 0; }
    static int enumParams(void*, int, std::uint32_t, std::uint32_t, std::uint32_t, const spa_pod*) { return -ENOTSUP; }
    static int setParam(void*, std::uint32_t, std::uint32_t, const spa_pod*) { return -ENOTSUP; }

    static void onDeviceBound(void* data, std::uint32_t globalId)
    {
        auto* self = static_cast<FakeAlsaDevice*>(data);
        self->deviceGlobalId_ = globalId;
        pw_thread_loop_signal(self->loop_, false);
    }
    static void onNodeBound(void* data, std::uint32_t)
    {
        auto* self = static_cast<FakeAlsaDevice*>(data);
        self->nodeBound_ = true;
        pw_thread_loop_signal(self->loop_, false);
    }

    static constexpr spa_device_methods deviceMethods = {
        .version = SPA_VERSION_DEVICE_METHODS,
        .add_listener = addListener,
        .sync = sync,
        .enum_params = enumParams,
        .set_param = setParam,
    };
    static constexpr pw_proxy_events deviceProxyEvents = {
        .version = PW_VERSION_PROXY_EVENTS,
        .destroy = nullptr,
        .bound = onDeviceBound,
        .removed = nullptr,
        .done = nullptr,
        .error = nullptr,
        .bound_props = nullptr,
    };
    static constexpr pw_proxy_events nodeProxyEvents = {
        .version = PW_VERSION_PROXY_EVENTS,
        .destroy = nullptr,
        .bound = onNodeBound,
        .removed = nullptr,
        .done = nullptr,
        .error = nullptr,
        .bound_props = nullptr,
    };

    std::string deviceName_;
    std::string vendorId_;
    std::string productId_;
    pw_thread_loop* loop_ = nullptr;
    pw_context* context_ = nullptr;
    pw_core* core_ = nullptr;
    pw_proxy* deviceProxy_ = nullptr;
    pw_proxy* nodeProxy_ = nullptr;
    spa_device device_{};
    spa_hook_list hooks_{};
    spa_dict infoProps_{};
    spa_dict_item itemsStorage_[5]{};
    spa_hook proxyListener_{};
    spa_hook nodeListener_{};
    std::uint32_t deviceGlobalId_ = 0;
    bool nodeBound_ = false;
};

} // namespace ghub::audio_processing::testing
