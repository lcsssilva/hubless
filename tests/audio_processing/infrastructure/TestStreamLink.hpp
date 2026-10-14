#pragma once

// Cliente de teste que, no daemon PRIVADO, cria um nó fonte (support.null-audio-sink com portas de saída) e um Link
// desse nó para um nó de entrada dado, imitando um fluxo de aplicativo ligado ao sink do EQ. Nunca usar fora do
// daemon privado.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <array>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/port-config.h>
#include <spa/pod/builder.h>

namespace ghub::audio_processing::testing {

class TestStreamLink {
public:
    explicit TestStreamLink(const std::string& socketPath)
    {
        pw_init(nullptr, nullptr);
        loop_ = pw_thread_loop_new("test-stream-link", nullptr);
        pw_thread_loop_start(loop_);
        pw_thread_loop_lock(loop_);
        context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): API C variádica
        core_ = pw_context_connect(context_, pw_properties_new(PW_KEY_REMOTE_NAME, socketPath.c_str(), nullptr), 0);
        if (core_ == nullptr) {
            pw_thread_loop_unlock(loop_);
            throw std::runtime_error("TestStreamLink: não conectou ao daemon privado");
        }
        registry_ = pw_core_get_registry(core_, PW_VERSION_REGISTRY, 0);
        pw_registry_add_listener(registry_, &registryListener_, &registryEvents, this);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): API C variádica
        pw_properties* props = pw_properties_new(
            "factory.name", "support.null-audio-sink", PW_KEY_NODE_NAME, "test.stream.source", PW_KEY_MEDIA_CLASS,
            "Audio/Sink", "audio.position", "[ FL FR ]", "adapter.auto-port-config",
            "{ mode = dsp monitor = true control = false position = preserve }", nullptr);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): create_object devolve void*
        sourceProxy_ = static_cast<pw_proxy*>(
            pw_core_create_object(core_, "adapter", PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, &props->dict, 0));
        pw_properties_free(props);
        if (sourceProxy_ == nullptr) {
            pw_thread_loop_unlock(loop_);
            throw std::runtime_error("TestStreamLink: não criou o nó fonte");
        }
        pw_proxy_add_listener(sourceProxy_, &sourceListener_, &sourceEvents, this);
        if (!waitFor([this] { return sourceId_ != 0; })) {
            pw_thread_loop_unlock(loop_);
            throw std::runtime_error("TestStreamLink: o nó fonte não recebeu id");
        }
        pw_thread_loop_unlock(loop_);
    }

    ~TestStreamLink()
    {
        pw_thread_loop_lock(loop_);
        spa_hook_remove(&sourceListener_);
        spa_hook_remove(&registryListener_);
        if (configuredNode_ != nullptr) {
            pw_proxy_destroy(configuredNode_);
        }
        if (linkListening_) {
            spa_hook_remove(&linkListener_);
        }
        if (linkProxy_ != nullptr) {
            pw_proxy_destroy(linkProxy_);
        }
        pw_proxy_destroy(sourceProxy_);
        pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry_)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        pw_core_disconnect(core_);
        pw_context_destroy(context_);
        pw_thread_loop_unlock(loop_);
        pw_thread_loop_stop(loop_);
        pw_thread_loop_destroy(loop_);
    }

    TestStreamLink(const TestStreamLink&) = delete;
    TestStreamLink& operator=(const TestStreamLink&) = delete;
    TestStreamLink(TestStreamLink&&) = delete;
    TestStreamLink& operator=(TestStreamLink&&) = delete;

    // Liga a fonte ao nó de entrada `inputNodeId`; volta quando o Link existe no daemon.
    void link(std::uint32_t inputNodeId)
    {
        pw_thread_loop_lock(loop_);
        configureInputPorts(inputNodeId);
        if (!waitFor([&] { return hasPort(inputNodeId, "in") && hasPort(sourceId_, "out"); })) {
            for (const auto& port : ports_) {
                std::fprintf(stderr, "porta node=%u dir=%s (fonte=%u entrada=%u)\n", port.first, port.second.c_str(),
                             sourceId_, inputNodeId);
            }
            pw_thread_loop_unlock(loop_);
            throw std::runtime_error("TestStreamLink: os nós não ganharam portas");
        }
        createLink(sourceId_, inputNodeId);
    }

    // Liga a saída do nó `outputNodeId` (por exemplo uma fonte Audio/Source) ao nó de teste, como um aplicativo que
    // grava; volta quando o Link existe no daemon.
    void linkFromOutput(std::uint32_t outputNodeId)
    {
        pw_thread_loop_lock(loop_);
        configurePorts(outputNodeId, SPA_DIRECTION_OUTPUT);
        if (!waitFor([&] { return hasPort(outputNodeId, "out") && hasPort(sourceId_, "in"); })) {
            pw_thread_loop_unlock(loop_);
            throw std::runtime_error("TestStreamLink: os nós não ganharam portas (saída)");
        }
        createLink(outputNodeId, sourceId_);
    }

    // Destrói o link (pode ser chamado de outra thread).
    void unlink()
    {
        pw_thread_loop_lock(loop_);
        if (linkListening_) {
            spa_hook_remove(&linkListener_);
            linkListening_ = false;
        }
        if (linkProxy_ != nullptr) {
            pw_proxy_destroy(linkProxy_);
            linkProxy_ = nullptr;
        }
        pw_thread_loop_unlock(loop_);
    }

private:
    // Chamado com o lock tomado; solta o lock antes de voltar.
    void createLink(std::uint32_t outputNode, std::uint32_t inputNode)
    {
        const std::string output = std::to_string(outputNode);
        const std::string input = std::to_string(inputNode);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): API C variádica
        pw_properties* props =
            pw_properties_new(PW_KEY_LINK_OUTPUT_NODE, output.c_str(), PW_KEY_LINK_INPUT_NODE, input.c_str(), nullptr);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): create_object devolve void*
        linkProxy_ = static_cast<pw_proxy*>(
            pw_core_create_object(core_, "link-factory", PW_TYPE_INTERFACE_Link, PW_VERSION_LINK, &props->dict, 0));
        pw_properties_free(props);
        if (linkProxy_ == nullptr) {
            pw_thread_loop_unlock(loop_);
            throw std::runtime_error("TestStreamLink: não criou o link");
        }
        pw_proxy_add_listener(linkProxy_, &linkListener_, &linkEvents, this);
        linkListening_ = true;
        const bool ok = waitFor([this] { return linkBound_; });
        pw_thread_loop_unlock(loop_);
        if (!ok) {
            throw std::runtime_error("TestStreamLink: o link não recebeu id");
        }
    }

    void configureInputPorts(std::uint32_t nodeId) { configurePorts(nodeId, SPA_DIRECTION_INPUT); }

    // Sem gerenciador de sessão ninguém configura as portas do nó de entrada: faz o que o WirePlumber faria.
    void configurePorts(std::uint32_t nodeId, spa_direction direction)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): bind devolve void*
        auto* node =
            static_cast<pw_node*>(pw_registry_bind(registry_, nodeId, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0));
        std::array<std::uint8_t, 1024> buffer{};
        spa_pod_builder builder{};
        spa_pod_builder_init(&builder, buffer.data(), static_cast<std::uint32_t>(buffer.size()));
        spa_audio_info_raw info{};
        info.format = SPA_AUDIO_FORMAT_F32P;
        info.rate = 48000;
        info.channels = 2;
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;
        const spa_pod* format = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info);
        const spa_pod* param = static_cast<const spa_pod*>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_ParamPortConfig, SPA_PARAM_PortConfig, SPA_PARAM_PORT_CONFIG_direction,
            SPA_POD_Id(direction), SPA_PARAM_PORT_CONFIG_mode, SPA_POD_Id(SPA_PARAM_PORT_CONFIG_MODE_dsp),
            SPA_PARAM_PORT_CONFIG_monitor, SPA_POD_Bool(false), SPA_PARAM_PORT_CONFIG_format, SPA_POD_Pod(format)));
        pw_node_set_param(node, SPA_PARAM_PortConfig, 0, param);
        configuredNode_ = reinterpret_cast<pw_proxy*>(node); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }

    [[nodiscard]] bool hasPort(std::uint32_t nodeId, const std::string& direction) const
    {
        for (const auto& port : ports_) {
            if (port.first == nodeId && port.second == direction) {
                return true;
            }
        }
        return false;
    }

    static void onGlobal(void* data, std::uint32_t, std::uint32_t, const char* type, std::uint32_t,
                         const spa_dict* props)
    {
        auto* self = static_cast<TestStreamLink*>(data);
        if (type == nullptr || props == nullptr || std::string_view{type} != PW_TYPE_INTERFACE_Port) {
            return;
        }
        const char* node = spa_dict_lookup(props, PW_KEY_NODE_ID);
        const char* direction = spa_dict_lookup(props, PW_KEY_PORT_DIRECTION);
        if (node != nullptr && direction != nullptr) {
            self->ports_.emplace_back(static_cast<std::uint32_t>(std::stoul(node)), direction);
            pw_thread_loop_signal(self->loop_, false);
        }
    }

    static constexpr pw_registry_events registryEvents = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = onGlobal,
        .global_remove = nullptr,
    };

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
            (void)pw_thread_loop_timed_wait_full(loop_, &abstime);
        }
        return true;
    }

    static void onSourceBound(void* data, std::uint32_t globalId)
    {
        auto* self = static_cast<TestStreamLink*>(data);
        self->sourceId_ = globalId;
        pw_thread_loop_signal(self->loop_, false);
    }
    static void onLinkBound(void* data, std::uint32_t)
    {
        auto* self = static_cast<TestStreamLink*>(data);
        self->linkBound_ = true;
        pw_thread_loop_signal(self->loop_, false);
    }

    static constexpr pw_proxy_events sourceEvents = {
        .version = PW_VERSION_PROXY_EVENTS,
        .destroy = nullptr,
        .bound = onSourceBound,
        .removed = nullptr,
        .done = nullptr,
        .error = nullptr,
        .bound_props = nullptr,
    };
    static void onLinkError(void*, int, int res, const char* message)
    {
        std::fprintf(stderr, "link error %d: %s\n", res, message != nullptr ? message : "");
    }
    static constexpr pw_proxy_events linkEvents = {
        .version = PW_VERSION_PROXY_EVENTS,
        .destroy = nullptr,
        .bound = onLinkBound,
        .removed = nullptr,
        .done = nullptr,
        .error = onLinkError,
        .bound_props = nullptr,
    };

    pw_thread_loop* loop_ = nullptr;
    pw_context* context_ = nullptr;
    pw_core* core_ = nullptr;
    pw_proxy* sourceProxy_ = nullptr;
    pw_proxy* linkProxy_ = nullptr;
    pw_registry* registry_ = nullptr;
    pw_proxy* configuredNode_ = nullptr;
    spa_hook registryListener_{};
    std::vector<std::pair<std::uint32_t, std::string>> ports_;
    spa_hook sourceListener_{};
    spa_hook linkListener_{};
    bool linkListening_ = false;
    std::uint32_t sourceId_ = 0;
    bool linkBound_ = false;
};

} // namespace ghub::audio_processing::testing
