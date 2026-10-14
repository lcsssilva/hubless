#include "modules/audio_processing/infrastructure/PipeWireAudioGraph.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

#include <pipewire/extensions/metadata.h>
#include <pipewire/impl.h>
#include <pipewire/pipewire.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>

#include "modules/audio_processing/infrastructure/PipeWireFilterChainArgs.hpp"
#include "modules/audio_processing/infrastructure/PipeWireGraphQueries.hpp"

namespace ghub::audio_processing {
namespace {

using shared_kernel::ErrorCode;
using shared_kernel::fail;

constexpr const char* filterChainModule = "libpipewire-module-filter-chain";
// Únicas chaves de metadata que o módulo escreve (R8).
constexpr const char* defaultSinkKey = "default.configured.audio.sink";
constexpr const char* defaultSourceKey = "default.configured.audio.source";
constexpr const char* metadataValueType = "Spa:String:JSON";

void initPipeWireOnce()
{
    static std::once_flag once;
    std::call_once(once, [] { pw_init(nullptr, nullptr); });
}

std::string lookup(const spa_dict* props, const char* key)
{
    if (props == nullptr) {
        return {};
    }
    const char* value = spa_dict_lookup(props, key);
    return value == nullptr ? std::string{} : std::string{value};
}

std::map<std::string, std::string> copyProps(const spa_dict* props)
{
    std::map<std::string, std::string> out;
    if (props != nullptr) {
        const spa_dict_item* item = nullptr;
        spa_dict_for_each(item, props)
        {
            if (item->key != nullptr && item->value != nullptr) {
                out[item->key] = item->value;
            }
        }
    }
    return out;
}

} // namespace

// Todo o estado abaixo é protegido pelo lock do pw_thread_loop (pw_thread_loop_lock), inclusive nos callbacks, que
// rodam na thread do loop com o lock já tomado.
class PipeWireAudioGraph::Impl {
public:
    Impl(std::string remoteName, std::chrono::milliseconds deadline)
        : remoteName_(std::move(remoteName)), deadline_(deadline)
    {
        equalizer_.owner = this;
        equalizer_.kind = ChainKind::Equalizer;
        equalizer_.publicNode = "ghub.g733.eq";
        equalizer_.otherNode = "ghub.g733.eq.output";
        equalizer_.streamsEnterPublicNode = true;
        equalizer_.alreadyExists = "o sink do EQ já existe";
        equalizer_.busy = "já existe um sink com o nome do EQ (outra instância?)";
        equalizer_.loadFailed = "não foi possível carregar o módulo filter-chain";
        equalizer_.notAppeared = "o sink do EQ não apareceu no PipeWire dentro do prazo";
        equalizer_.proxyFailed = "não foi possível acessar o sink do EQ";
        equalizer_.missing = "o sink do EQ não existe";
        equalizer_.paramsBuild = "não foi possível montar os parâmetros do EQ";
        equalizer_.paramsSend = "falha ao enviar os parâmetros ao sink do EQ";
        equalizer_.paramsRefused = "o PipeWire recusou os parâmetros do EQ: ";
        equalizer_.removedReason = "o sink do EQ foi removido do PipeWire";
        equalizer_.destroyedReason = "o módulo do EQ foi destruído";

        noise_.owner = this;
        noise_.kind = ChainKind::NoiseSuppression;
        noise_.publicNode = "ghub.g733.mic";
        noise_.otherNode = "ghub.g733.mic.input";
        noise_.streamsEnterPublicNode = false;
        noise_.alreadyExists = "a fonte com redução de ruído já existe";
        noise_.busy = "já existe uma fonte com o nome da redução de ruído (outra instância?)";
        noise_.loadFailed = "plugin de redução de ruído não instalado (noise-suppression-for-voice)";
        noise_.notAppeared = "a fonte com redução de ruído não apareceu no PipeWire dentro do prazo";
        noise_.proxyFailed = "não foi possível acessar a fonte com redução de ruído";
        noise_.missing = "a fonte com redução de ruído não existe";
        noise_.paramsBuild = "não foi possível montar os parâmetros da redução de ruído";
        noise_.paramsSend = "falha ao enviar os parâmetros à fonte com redução de ruído";
        noise_.paramsRefused = "o PipeWire recusou os parâmetros da redução de ruído: ";
        noise_.removedReason = "a fonte com redução de ruído foi removida do PipeWire";
        noise_.destroyedReason = "o módulo da redução de ruído foi destruído";
    }

    ~Impl()
    {
        if (loop_ != nullptr) {
            pw_thread_loop_lock(loop_);
            waitForStreamsToLeaveLocked(true, true);
            pw_thread_loop_unlock(loop_);
        }
        teardown();
    }
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    shared_kernel::Result<std::string> findHeadsetSink()
    {
        const Guard guard{*this};
        if (!guard.ok()) {
            return fail(ErrorCode::BackendUnavailable, "não foi possível iniciar o loop do PipeWire");
        }
        if (auto ready = ensureConnected(); !ready) {
            return std::unexpected(ready.error());
        }
        auto snapshot = snapshotLocked();
        if (!snapshot) {
            return std::unexpected(snapshot.error());
        }
        return findHeadsetSinkName(*snapshot);
    }

    shared_kernel::Result<std::string> findHeadsetSource()
    {
        const Guard guard{*this};
        if (!guard.ok()) {
            return fail(ErrorCode::BackendUnavailable, "não foi possível iniciar o loop do PipeWire");
        }
        if (auto ready = ensureConnected(); !ready) {
            return std::unexpected(ready.error());
        }
        auto snapshot = snapshotLocked();
        if (!snapshot) {
            return std::unexpected(snapshot.error());
        }
        return findHeadsetSourceName(*snapshot);
    }

    shared_kernel::Result<void> createEqualizer(const EqualizerCurve& curve, const std::string& targetSink)
    {
        const Guard guard{*this};
        if (!guard.ok()) {
            return fail(ErrorCode::BackendUnavailable, "não foi possível iniciar o loop do PipeWire");
        }
        if (auto ready = ensureConnected(); !ready) {
            return ready;
        }
        if (auto prepared = prepareCreateLocked(equalizer_); !prepared) {
            return prepared;
        }
        const auto args = buildFilterChainArgs(curve, targetSink, remoteName_);
        if (!args) {
            return std::unexpected(args.error());
        }
        return loadChainLocked(equalizer_, *args);
    }

    shared_kernel::Result<void> updateEqualizer(const EqualizerCurve& curve)
    {
        return updateChain(equalizer_, buildControlUpdates(curve));
    }

    shared_kernel::Result<void> removeEqualizer() { return removeChain(equalizer_); }

    shared_kernel::Result<void> createNoiseSuppression(VoiceThreshold threshold, const std::string& source)
    {
        const Guard guard{*this};
        if (!guard.ok()) {
            return fail(ErrorCode::BackendUnavailable, "não foi possível iniciar o loop do PipeWire");
        }
        if (auto ready = ensureConnected(); !ready) {
            return ready;
        }
        if (auto prepared = prepareCreateLocked(noise_); !prepared) {
            return prepared;
        }
        const auto args = buildNoiseSuppressionArgs(threshold, source, remoteName_);
        if (!args) {
            return std::unexpected(args.error());
        }
        return loadChainLocked(noise_, *args);
    }

    shared_kernel::Result<void> updateNoiseSuppression(VoiceThreshold threshold)
    {
        return updateChain(noise_, buildNoiseSuppressionControlUpdates(threshold));
    }

    shared_kernel::Result<void> removeNoiseSuppression() { return removeChain(noise_); }

    shared_kernel::Result<std::optional<std::string>> configuredDefault(const char* key)
    {
        const Guard guard{*this};
        if (!guard.ok()) {
            return fail(ErrorCode::BackendUnavailable, "não foi possível iniciar o loop do PipeWire");
        }
        if (auto ready = ensureConnected(); !ready) {
            return std::unexpected(ready.error());
        }
        if (metadata_ == nullptr) {
            return fail(ErrorCode::BackendUnavailable, "metadata default ausente no PipeWire");
        }
        const auto it = metadataValues_.find(key);
        if (it == metadataValues_.end()) {
            return std::optional<std::string>{};
        }
        auto name = parseDefaultNodeValue(it->second, key);
        if (!name) {
            return std::unexpected(name.error());
        }
        return std::optional<std::string>{*std::move(name)};
    }

    shared_kernel::Result<void> setConfiguredDefault(const char* key, const std::optional<std::string>& nodeName)
    {
        const Guard guard{*this};
        if (!guard.ok()) {
            return fail(ErrorCode::BackendUnavailable, "não foi possível iniciar o loop do PipeWire");
        }
        if (auto ready = ensureConnected(); !ready) {
            return ready;
        }
        if (metadata_ == nullptr) {
            return fail(ErrorCode::BackendUnavailable, "metadata default ausente no PipeWire");
        }
        const Deadline deadline = makeDeadline();
        const std::string value = nodeName ? makeDefaultNodeValue(*nodeName) : std::string{};
        const int res = nodeName ? writeDefaultLocked(key, metadataValueType, value.c_str())
                                 : writeDefaultLocked(key, nullptr, nullptr);
        if (res < 0) {
            return fail(ErrorCode::BackendUnavailable, "falha ao gravar a preferência de áudio padrão no PipeWire");
        }
        // Confirma pelo eco do servidor: a propriedade volta pelo listener `property`.
        const std::string keyName = key;
        const bool confirmed = waitUntil(deadline, [&] {
            const auto it = metadataValues_.find(keyName);
            return !connected_ ||
                   (nodeName ? (it != metadataValues_.end() && it->second == value) : it == metadataValues_.end());
        });
        if (!confirmed || !connected_) {
            return fail(ErrorCode::BackendUnavailable,
                        "o PipeWire não confirmou a preferência de áudio padrão dentro do prazo");
        }
        return {};
    }

    void setObserver(AudioGraphObserver* observer) { replaceObserver(equalizerObserver_, observer); }
    void setNoiseSuppressionObserver(NoiseSuppressionObserver* observer) { replaceObserver(noiseObserver_, observer); }

    // Um bind de Device por vez de vida curta: o listener de `info` guarda as props completas.
    struct DeviceProbe {
        Impl* owner = nullptr;
        std::uint32_t id = 0;
        pw_device* proxy = nullptr;
        spa_hook listener{};
        std::map<std::string, std::string> props;
    };

private:
    static constexpr std::chrono::seconds streamDrainDeadline{2};
    using Clock = std::chrono::steady_clock;
    using Deadline = Clock::time_point;

    enum class ChainKind : std::uint8_t { Equalizer, NoiseSuppression };

    // Uma instância de filter-chain (EQ da saída ou redução de ruído do microfone): módulo carregado no processo, nó
    // público (o que o usuário escolhe como padrão), nó interno do outro lado, estado de perda próprio. As frases são
    // as mensagens de erro/perda da instância.
    struct Chain {
        Impl* owner = nullptr;
        ChainKind kind = ChainKind::Equalizer;
        const char* publicNode = "";
        const char* otherNode = "";
        // true: Links que ENTRAM no nó público (sink do EQ); false: Links que SAEM dele (fonte filtrada).
        bool streamsEnterPublicNode = true;
        const char* alreadyExists = "";
        const char* busy = "";
        const char* loadFailed = "";
        const char* notAppeared = "";
        const char* proxyFailed = "";
        const char* missing = "";
        const char* paramsBuild = "";
        const char* paramsSend = "";
        const char* paramsRefused = "";
        const char* removedReason = "";
        const char* destroyedReason = "";

        pw_impl_module* module = nullptr;
        pw_node* proxy = nullptr;
        spa_hook moduleListener{};
        spa_hook proxyListener{};
        bool proxyListening = false;
        bool ownDestroy = false;
        bool lostReported = false;
        std::uint32_t publicId = 0;
        std::uint32_t otherId = 0;
        std::string proxyError;
    };

    struct LinkEnds {
        std::uint32_t output = 0;
        std::uint32_t input = 0;
    };

    // Lock do thread loop, mas só se já existir (a primeira operação cria o loop em ensureConnected).
    class Guard {
    public:
        explicit Guard(Impl& impl) : impl_(impl), locked_(impl_.openLoop()) {}
        ~Guard()
        {
            if (locked_) {
                pw_thread_loop_unlock(impl_.loop_);
            }
        }
        [[nodiscard]] bool ok() const noexcept { return locked_; }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
        Guard(Guard&&) = delete;
        Guard& operator=(Guard&&) = delete;

    private:
        Impl& impl_;
        bool locked_;
    };

    template <typename Observer> void replaceObserver(Observer*& slot, Observer* observer)
    {
        {
            const std::scoped_lock lock{loopMutex_};
            if (loop_ == nullptr) {
                slot = observer; // ainda não há thread do loop que possa usar o ponteiro
                return;
            }
        }
        // Dentro de um callback o lock do loop já está tomado (e não pode ser tomado de novo com segurança).
        if (pw_thread_loop_in_thread(loop_)) {
            slot = observer;
            return;
        }
        // Fora do loop: depois do retorno nenhum callback em andamento ou futuro usa o observador antigo.
        pw_thread_loop_lock(loop_);
        slot = observer;
        pw_thread_loop_unlock(loop_);
    }

    static void onDeviceInfo(void* data, const pw_device_info* info)
    {
        auto* probe = static_cast<DeviceProbe*>(data);
        if (info != nullptr && info->props != nullptr) {
            for (auto& [key, value] : copyProps(info->props)) {
                probe->props[key] = std::move(value);
            }
        }
    }

    static constexpr pw_device_events deviceEvents = {
        .version = PW_VERSION_DEVICE_EVENTS,
        .info = onDeviceInfo,
        .param = nullptr,
    };

    // Nós e dispositivos do registro. O evento global do registro não traz device.vendor.id/device.product.id
    // (impl-device.c:30-41@1.6.9): só o evento `info` do Device. Faz bind de cada Audio/Device (ALSA), lê o info e
    // solta os proxies.
    shared_kernel::Result<GraphSnapshot> snapshotLocked()
    {
        GraphSnapshot snapshot;
        for (const auto& [id, props] : nodes_) {
            snapshot.nodes.push_back({.id = id, .props = props});
        }
        auto infos = collectDeviceInfoLocked(makeDeadline());
        if (!infos) {
            return std::unexpected(infos.error());
        }
        for (auto& [id, props] : *infos) {
            snapshot.devices.push_back({.id = id, .props = std::move(props)});
        }
        return snapshot;
    }

    shared_kernel::Result<std::map<std::uint32_t, std::map<std::string, std::string>>>
    collectDeviceInfoLocked(const std::chrono::steady_clock::time_point& deadline)
    {
        std::vector<std::unique_ptr<DeviceProbe>> probes;
        for (const auto& [id, props] : devices_) {
            const auto mediaClass = props.find("media.class");
            const auto api = props.find("device.api");
            if (mediaClass == props.end() || mediaClass->second != "Audio/Device" ||
                (api != props.end() && api->second != "alsa")) {
                continue;
            }
            auto probe = std::make_unique<DeviceProbe>();
            probe->owner = this;
            probe->id = id;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): pw_registry_bind devolve void* do proxy
            probe->proxy = static_cast<pw_device*>(
                pw_registry_bind(registry_, id, PW_TYPE_INTERFACE_Device, PW_VERSION_DEVICE, 0));
            if (probe->proxy == nullptr) {
                continue;
            }
            pw_device_add_listener(probe->proxy, &probe->listener, &deviceEvents, probe.get());
            probes.push_back(std::move(probe));
        }
        const auto synced = roundtrip(deadline);
        std::map<std::uint32_t, std::map<std::string, std::string>> result;
        for (auto& probe : probes) {
            spa_hook_remove(&probe->listener);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): pw_device é um pw_proxy
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(probe->proxy));
            result[probe->id] = std::move(probe->props);
        }
        if (!synced) {
            return std::unexpected(synced.error());
        }
        return result;
    }

    // Verificações de criação (instância livre, nome livre no grafo) e limpeza de resíduos de uma instância perdida.
    shared_kernel::Result<void> prepareCreateLocked(Chain& chain)
    {
        if (chain.module != nullptr) {
            return fail(ErrorCode::InvalidArgument, chain.alreadyExists);
        }
        if (findNodeIdLocked(chain.publicNode) != 0) {
            return fail(ErrorCode::DeviceBusy, chain.busy);
        }
        destroyChainLocked(chain); // resíduos de uma instância perdida antes (proxy do nó)
        return {};
    }

    shared_kernel::Result<void> loadChainLocked(Chain& chain, const std::string& args)
    {
        const Deadline deadline = makeDeadline();
        chain.lostReported = false;
        chain.publicId = 0;
        chain.otherId = 0;
        chain.module = pw_context_load_module(context_, filterChainModule, args.c_str(), nullptr);
        if (chain.module == nullptr) {
            return fail(ErrorCode::BackendUnavailable, chain.loadFailed);
        }
        pw_impl_module_add_listener(chain.module, &chain.moduleListener, &moduleEvents, &chain);

        const bool appeared =
            waitUntil(deadline, [&] { return !connected_ || chain.module == nullptr || chain.publicId != 0; });
        if (!appeared || !connected_ || chain.module == nullptr || chain.publicId == 0) {
            destroyChainLocked(chain);
            return fail(ErrorCode::BackendUnavailable, chain.notAppeared);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): pw_registry_bind devolve void* do proxy
        chain.proxy = static_cast<pw_node*>(
            pw_registry_bind(registry_, chain.publicId, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0));
        if (chain.proxy == nullptr) {
            destroyChainLocked(chain);
            return fail(ErrorCode::BackendUnavailable, chain.proxyFailed);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): pw_node é um pw_proxy na API C
        pw_proxy_add_listener(reinterpret_cast<pw_proxy*>(chain.proxy), &chain.proxyListener, &proxyEvents, &chain);
        chain.proxyListening = true;
        if (auto synced = roundtrip(deadline); !synced) {
            destroyChainLocked(chain);
            return synced;
        }
        return {};
    }

    shared_kernel::Result<void> updateChain(Chain& chain, const std::vector<FilterChainControl>& controls)
    {
        const Guard guard{*this};
        if (!guard.ok()) {
            return fail(ErrorCode::BackendUnavailable, "não foi possível iniciar o loop do PipeWire");
        }
        if (auto ready = ensureConnected(); !ready) {
            return ready;
        }
        if (chain.module == nullptr || chain.proxy == nullptr) {
            return fail(ErrorCode::InvalidArgument, chain.missing);
        }
        std::array<std::uint8_t, 2048> buffer{};
        spa_pod_builder builder{};
        spa_pod_builder_init(&builder, buffer.data(), static_cast<std::uint32_t>(buffer.size()));
        spa_pod_frame objectFrame{};
        spa_pod_frame structFrame{};
        spa_pod_builder_push_object(&builder, &objectFrame, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props);
        spa_pod_builder_prop(&builder, SPA_PROP_params, 0);
        spa_pod_builder_push_struct(&builder, &structFrame);
        for (const auto& control : controls) {
            spa_pod_builder_string(&builder, control.name.c_str());
            spa_pod_builder_float(&builder, control.value);
        }
        spa_pod_builder_pop(&builder, &structFrame);
        const auto* param = static_cast<spa_pod*>(spa_pod_builder_pop(&builder, &objectFrame));
        if (param == nullptr) {
            return fail(ErrorCode::InvalidArgument, chain.paramsBuild);
        }

        const Deadline deadline = makeDeadline();
        chain.proxyError.clear();
        if (pw_node_set_param(chain.proxy, SPA_PARAM_Props, 0, param) < 0) {
            return fail(ErrorCode::BackendUnavailable, chain.paramsSend);
        }
        if (auto synced = roundtrip(deadline); !synced) {
            return synced;
        }
        if (!chain.proxyError.empty()) {
            return fail(ErrorCode::BackendUnavailable, chain.paramsRefused + chain.proxyError);
        }
        return {};
    }

    shared_kernel::Result<void> removeChain(Chain& chain)
    {
        if (loop_ == nullptr) {
            return {};
        }
        const Guard guard{*this};
        if (!guard.ok()) {
            return {};
        }
        const bool had = chain.module != nullptr;
        waitForStreamsToLeaveLocked(&chain == &equalizer_, &chain == &noise_);
        destroyChainLocked(chain);
        if (had && connected_) {
            // O retorno é ignorado de propósito: a destruição do módulo já foi feita localmente (o módulo desconecta o
            // próprio core), então um prazo esgotado aqui não deixa a instância viva; só não confirmamos o eco do
            // servidor.
            // NOLINTNEXTLINE(bugprone-unused-return-value)
            (void)roundtrip(makeDeadline());
        }
        return {};
    }

    // Única porta de escrita de metadata do módulo (R8): só as duas chaves configuradas, cada uma com chamada própria.
    int writeDefaultLocked(const char* key, const char* type, const char* value)
    {
        if (std::string_view{key} == defaultSourceKey) {
            return pw_metadata_set_property(metadata_, 0, defaultSourceKey, type, value);
        }
        return pw_metadata_set_property(metadata_, 0, defaultSinkKey, type, value);
    }

    [[nodiscard]] Deadline makeDeadline() const { return Clock::now() + deadline_; }

    template <typename Predicate> bool waitUntil(const Deadline& deadline, Predicate done)
    {
        while (!done()) {
            const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - Clock::now());
            if (remaining.count() <= 0) {
                return false;
            }
            timespec abstime{};
            pw_thread_loop_get_time(loop_, &abstime, remaining.count());
            (void)pw_thread_loop_timed_wait_full(loop_, &abstime);
        }
        return true;
    }

    shared_kernel::Result<void> roundtrip(const Deadline& deadline)
    {
        if (!connected_) {
            return fail(ErrorCode::BackendUnavailable, "sem conexão com o PipeWire");
        }
        const int seq = pw_core_sync(core_, PW_ID_CORE, 0);
        if (seq < 0) {
            return fail(ErrorCode::BackendUnavailable, "falha ao sincronizar com o PipeWire");
        }
        pendingSeq_ = seq;
        const bool done = waitUntil(deadline, [this] { return !connected_ || doneSeq_ == pendingSeq_; });
        if (!done || !connected_) {
            return fail(ErrorCode::BackendUnavailable, "o PipeWire não respondeu dentro do prazo");
        }
        return {};
    }

    // Cria o loop (uma vez) e toma o lock; false se o loop não pôde ser criado.
    bool openLoop()
    {
        const std::scoped_lock lock{loopMutex_};
        if (loop_ == nullptr) {
            initPipeWireOnce();
            loop_ = pw_thread_loop_new("ghub-audio-processing", nullptr);
            if (loop_ != nullptr && pw_thread_loop_start(loop_) < 0) {
                pw_thread_loop_destroy(loop_);
                loop_ = nullptr;
            }
        }
        if (loop_ == nullptr) {
            return false;
        }
        pw_thread_loop_lock(loop_);
        return true;
    }

    shared_kernel::Result<void> ensureConnected()
    {
        if (core_ != nullptr && connected_) {
            return {};
        }
        // Conexão perdida antes: descarta o que sobrou e tenta de novo.
        if (core_ != nullptr || context_ != nullptr) {
            teardownConnectionLocked();
        }
        const Deadline deadline = makeDeadline();
        context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
        if (context_ == nullptr) {
            return fail(ErrorCode::BackendUnavailable, "não foi possível criar o contexto do PipeWire");
        }
        pw_properties* props =
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): pw_properties_new é variádica na API C
            remoteName_.empty() ? nullptr : pw_properties_new(PW_KEY_REMOTE_NAME, remoteName_.c_str(), nullptr);
        core_ = pw_context_connect(context_, props, 0);
        if (core_ == nullptr) {
            teardownConnectionLocked();
            return fail(ErrorCode::BackendUnavailable, "não foi possível conectar ao PipeWire");
        }
        connected_ = true;
        pw_core_add_listener(core_, &coreListener_, &coreEvents, this);
        coreListening_ = true;
        registry_ = pw_core_get_registry(core_, PW_VERSION_REGISTRY, 0);
        if (registry_ == nullptr) {
            teardownConnectionLocked();
            return fail(ErrorCode::BackendUnavailable, "não foi possível abrir o registro do PipeWire");
        }
        pw_registry_add_listener(registry_, &registryListener_, &registryEvents, this);
        registryListening_ = true;
        // 1ª volta: globais iniciais (e bind do metadata default); 2ª volta: propriedades iniciais do metadata.
        for (int round = 0; round < 2; ++round) {
            if (auto synced = roundtrip(deadline); !synced) {
                teardownConnectionLocked();
                return synced;
            }
        }
        return {};
    }

    [[nodiscard]] bool hasStreamLinkLocked(const Chain& chain) const
    {
        return chain.publicId != 0 && std::ranges::any_of(links_, [&chain](const auto& link) {
                   return (chain.streamsEnterPublicNode ? link.second.input : link.second.output) == chain.publicId;
               });
    }

    // Antes de destruir o nó público, espera os fluxos (WirePlumber) saírem dele: destruir antes faz alguns
    // aplicativos (Spotify) pausarem. Prazo curto e próprio, comum às instâncias pedidas; fluxos fixados pelo usuário
    // nunca saem, e aí destrói assim mesmo.
    void waitForStreamsToLeaveLocked(bool equalizer, bool noise)
    {
        const auto pending = [this](const Chain& chain) {
            return chain.module != nullptr && hasStreamLinkLocked(chain);
        };
        if (!connected_) {
            return;
        }
        (void)waitUntil(Clock::now() + streamDrainDeadline, [&] {
            return !connected_ || !((equalizer && pending(equalizer_)) || (noise && pending(noise_)));
        });
    }

    static void destroyChainLocked(Chain& chain)
    {
        if (chain.proxyListening) {
            spa_hook_remove(&chain.proxyListener);
            chain.proxyListening = false;
        }
        if (chain.proxy != nullptr) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): proxy da API C do PipeWire
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(chain.proxy));
            chain.proxy = nullptr;
        }
        if (chain.module != nullptr) {
            pw_impl_module* module = chain.module;
            chain.module = nullptr; // o evento destroy do módulo vê "destruição própria"
            chain.ownDestroy = true;
            spa_hook_remove(&chain.moduleListener);
            pw_impl_module_destroy(module);
            chain.ownDestroy = false;
        }
        chain.publicId = 0;
        chain.otherId = 0;
    }

    void teardownConnectionLocked()
    {
        destroyChainLocked(equalizer_);
        destroyChainLocked(noise_);
        if (metadataListening_) {
            spa_hook_remove(&metadataListener_);
            metadataListening_ = false;
        }
        if (metadata_ != nullptr) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): proxy da API C do PipeWire
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(metadata_));
            metadata_ = nullptr;
        }
        if (registryListening_) {
            spa_hook_remove(&registryListener_);
            registryListening_ = false;
        }
        if (registry_ != nullptr) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): proxy da API C do PipeWire
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry_));
            registry_ = nullptr;
        }
        if (coreListening_) {
            spa_hook_remove(&coreListener_);
            coreListening_ = false;
        }
        if (core_ != nullptr) {
            pw_core_disconnect(core_);
            core_ = nullptr;
        }
        if (context_ != nullptr) {
            pw_context_destroy(context_);
            context_ = nullptr;
        }
        connected_ = false;
        nodes_.clear();
        devices_.clear();
        links_.clear();
        metadataValues_.clear();
        metadataId_ = 0;
        pendingSeq_ = -1;
        doneSeq_ = -2;
    }

    void teardown()
    {
        if (loop_ == nullptr) {
            return;
        }
        pw_thread_loop_lock(loop_);
        teardownConnectionLocked();
        pw_thread_loop_unlock(loop_);
        pw_thread_loop_stop(loop_);
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
    }

    [[nodiscard]] std::uint32_t findNodeIdLocked(const char* name) const
    {
        for (const auto& [id, props] : nodes_) {
            const auto it = props.find("node.name");
            if (it != props.end() && it->second == name) {
                return id;
            }
        }
        return 0;
    }

    // Chamado com o lock tomado (thread do loop). Notifica uma vez, só o observador da instância.
    void reportLostLocked(Chain& chain, const std::string& reason)
    {
        if (chain.lostReported || chain.module == nullptr || chain.ownDestroy) {
            return;
        }
        chain.lostReported = true;
        if (chain.kind == ChainKind::Equalizer) {
            if (equalizerObserver_ != nullptr) {
                equalizerObserver_->equalizerLost(reason);
            }
        } else if (noiseObserver_ != nullptr) {
            noiseObserver_->noiseSuppressionLost(reason);
        }
    }

    // --- callbacks (thread do loop, lock tomado) ---

    static void onCoreDone(void* data, std::uint32_t id, int seq)
    {
        auto* self = static_cast<Impl*>(data);
        if (id == PW_ID_CORE) {
            self->doneSeq_ = seq;
            pw_thread_loop_signal(self->loop_, false);
        }
    }

    static void onCoreError(void* data, std::uint32_t id, int /*seq*/, int res, const char* /*message*/)
    {
        auto* self = static_cast<Impl*>(data);
        if (id == PW_ID_CORE && res == -EPIPE) {
            self->connected_ = false;
            self->reportLostLocked(self->equalizer_, "conexão com o PipeWire perdida");
            self->reportLostLocked(self->noise_, "conexão com o PipeWire perdida");
            pw_thread_loop_signal(self->loop_, false);
        }
    }

    static void trackChainNode(Chain& chain, std::uint32_t id, const std::string& name)
    {
        if (chain.module == nullptr) {
            return;
        }
        if (name == chain.publicNode && chain.publicId == 0) {
            chain.publicId = id;
        } else if (name == chain.otherNode && chain.otherId == 0) {
            chain.otherId = id;
        }
    }

    static void onGlobal(void* data, std::uint32_t id, std::uint32_t /*permissions*/, const char* type,
                         std::uint32_t /*version*/, const spa_dict* props)
    {
        auto* self = static_cast<Impl*>(data);
        if (type == nullptr) {
            return;
        }
        const std::string_view kind{type};
        if (kind == PW_TYPE_INTERFACE_Node) {
            auto copy = copyProps(props);
            const auto name = copy.find("node.name");
            if (name != copy.end()) {
                trackChainNode(self->equalizer_, id, name->second);
                trackChainNode(self->noise_, id, name->second);
            }
            self->nodes_[id] = std::move(copy);
            pw_thread_loop_signal(self->loop_, false);
        } else if (kind == PW_TYPE_INTERFACE_Link) {
            // link.input.node e link.output.node fazem parte dos global_keys do Link (impl-link.c:1683-1692@1.6.9).
            const auto input = lookup(props, PW_KEY_LINK_INPUT_NODE);
            const auto output = lookup(props, PW_KEY_LINK_OUTPUT_NODE);
            if (!input.empty() || !output.empty()) {
                self->links_[id] = {
                    .output = output.empty() ? 0U : static_cast<std::uint32_t>(std::stoul(output)),
                    .input = input.empty() ? 0U : static_cast<std::uint32_t>(std::stoul(input)),
                };
                pw_thread_loop_signal(self->loop_, false);
            }
        } else if (kind == PW_TYPE_INTERFACE_Device) {
            self->devices_[id] = copyProps(props);
        } else if (kind == PW_TYPE_INTERFACE_Metadata && self->metadata_ == nullptr &&
                   lookup(props, "metadata.name") == "default") {
            self->metadata_ = static_cast<pw_metadata*>(
                pw_registry_bind(self->registry_, id, PW_TYPE_INTERFACE_Metadata, PW_VERSION_METADATA, 0));
            if (self->metadata_ != nullptr) {
                self->metadataId_ = id;
                pw_metadata_add_listener(self->metadata_, &self->metadataListener_, &metadataEvents, self);
                self->metadataListening_ = true;
            }
        }
    }

    void chainNodeRemoved(Chain& chain, std::uint32_t id)
    {
        if (chain.module != nullptr && chain.publicId != 0 && (id == chain.publicId || id == chain.otherId)) {
            reportLostLocked(chain, chain.removedReason);
        }
    }

    static void onGlobalRemove(void* data, std::uint32_t id)
    {
        auto* self = static_cast<Impl*>(data);
        self->nodes_.erase(id);
        self->devices_.erase(id);
        self->links_.erase(id);
        self->chainNodeRemoved(self->equalizer_, id);
        self->chainNodeRemoved(self->noise_, id);
        pw_thread_loop_signal(self->loop_, false);
    }

    static int onMetadataProperty(void* data, std::uint32_t subject, const char* key, const char* /*type*/,
                                  const char* value)
    {
        auto* self = static_cast<Impl*>(data);
        if (subject == 0 && key != nullptr) {
            if (value == nullptr) {
                self->metadataValues_.erase(key);
            } else {
                self->metadataValues_[key] = value;
            }
            pw_thread_loop_signal(self->loop_, false);
        } else if (key == nullptr && subject == 0) {
            self->metadataValues_.clear(); // clear de todo o metadata
        }
        return 0;
    }

    static void onModuleDestroy(void* data)
    {
        auto* chain = static_cast<Chain*>(data);
        Impl* self = chain->owner;
        if (!chain->ownDestroy && chain->module != nullptr) {
            // Destruição fora do nosso remove (por exemplo, erro do core interno do módulo).
            self->reportLostLocked(*chain, chain->destroyedReason);
            spa_hook_remove(&chain->moduleListener);
            chain->module = nullptr;
            pw_thread_loop_signal(self->loop_, false);
        }
    }

    static void onProxyError(void* data, int /*seq*/, int res, const char* message)
    {
        auto* chain = static_cast<Chain*>(data);
        chain->proxyError = message != nullptr ? message : "erro desconhecido";
        (void)res;
        pw_thread_loop_signal(chain->owner->loop_, false);
    }

    static constexpr pw_core_events coreEvents = {
        .version = PW_VERSION_CORE_EVENTS,
        .info = nullptr,
        .done = onCoreDone,
        .ping = nullptr,
        .error = onCoreError,
        .remove_id = nullptr,
        .bound_id = nullptr,
        .add_mem = nullptr,
        .remove_mem = nullptr,
        .bound_props = nullptr,
    };
    static constexpr pw_registry_events registryEvents = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = onGlobal,
        .global_remove = onGlobalRemove,
    };
    static constexpr pw_metadata_events metadataEvents = {
        .version = PW_VERSION_METADATA_EVENTS,
        .property = onMetadataProperty,
    };
    static constexpr pw_impl_module_events moduleEvents = {
        .version = PW_VERSION_IMPL_MODULE_EVENTS,
        .destroy = onModuleDestroy,
        .free = nullptr,
        .initialized = nullptr,
        .registered = nullptr,
    };
    static constexpr pw_proxy_events proxyEvents = {
        .version = PW_VERSION_PROXY_EVENTS,
        .destroy = nullptr,
        .bound = nullptr,
        .removed = nullptr,
        .done = nullptr,
        .error = onProxyError,
        .bound_props = nullptr,
    };

    std::mutex loopMutex_; // criação do loop vs. setObserver antes do loop existir
    const std::string remoteName_;
    const std::chrono::milliseconds deadline_;

    pw_thread_loop* loop_ = nullptr;
    pw_context* context_ = nullptr;
    pw_core* core_ = nullptr;
    pw_registry* registry_ = nullptr;
    pw_metadata* metadata_ = nullptr;

    spa_hook coreListener_{};
    spa_hook registryListener_{};
    spa_hook metadataListener_{};
    bool coreListening_ = false;
    bool registryListening_ = false;
    bool metadataListening_ = false;

    bool connected_ = false;
    int pendingSeq_ = -1;
    int doneSeq_ = -2;
    std::uint32_t metadataId_ = 0;

    Chain equalizer_;
    Chain noise_;
    AudioGraphObserver* equalizerObserver_ = nullptr;
    NoiseSuppressionObserver* noiseObserver_ = nullptr;

    std::map<std::uint32_t, std::map<std::string, std::string>> nodes_;
    std::map<std::uint32_t, std::map<std::string, std::string>> devices_;
    std::map<std::uint32_t, LinkEnds> links_; // id do Link -> nós de saída e de entrada
    std::map<std::string, std::string> metadataValues_;
};

PipeWireAudioGraph::PipeWireAudioGraph(std::string remoteName, std::chrono::milliseconds deadline)
    : impl_(std::make_unique<Impl>(std::move(remoteName), deadline))
{
}

PipeWireAudioGraph::~PipeWireAudioGraph() = default;

shared_kernel::Result<std::string> PipeWireAudioGraph::findHeadsetSink()
{
    return impl_->findHeadsetSink();
}

shared_kernel::Result<void> PipeWireAudioGraph::createEqualizer(const EqualizerCurve& curve,
                                                                const std::string& targetSink)
{
    return impl_->createEqualizer(curve, targetSink);
}

shared_kernel::Result<void> PipeWireAudioGraph::updateEqualizer(const EqualizerCurve& curve)
{
    return impl_->updateEqualizer(curve);
}

shared_kernel::Result<void> PipeWireAudioGraph::removeEqualizer()
{
    return impl_->removeEqualizer();
}

std::string PipeWireAudioGraph::equalizerSinkName() const
{
    return "ghub.g733.eq";
}

shared_kernel::Result<std::optional<std::string>> PipeWireAudioGraph::configuredDefaultSink()
{
    return impl_->configuredDefault(defaultSinkKey);
}

shared_kernel::Result<void> PipeWireAudioGraph::setConfiguredDefaultSink(const std::optional<std::string>& sinkName)
{
    return impl_->setConfiguredDefault(defaultSinkKey, sinkName);
}

void PipeWireAudioGraph::setObserver(AudioGraphObserver* observer)
{
    impl_->setObserver(observer);
}

shared_kernel::Result<std::string> PipeWireAudioGraph::findHeadsetSource()
{
    return impl_->findHeadsetSource();
}

shared_kernel::Result<void> PipeWireAudioGraph::createNoiseSuppression(VoiceThreshold threshold,
                                                                       const std::string& source)
{
    return impl_->createNoiseSuppression(threshold, source);
}

shared_kernel::Result<void> PipeWireAudioGraph::updateNoiseSuppression(VoiceThreshold threshold)
{
    return impl_->updateNoiseSuppression(threshold);
}

shared_kernel::Result<void> PipeWireAudioGraph::removeNoiseSuppression()
{
    return impl_->removeNoiseSuppression();
}

std::string PipeWireAudioGraph::noiseSuppressionSourceName() const
{
    return "ghub.g733.mic";
}

shared_kernel::Result<std::optional<std::string>> PipeWireAudioGraph::configuredDefaultSource()
{
    return impl_->configuredDefault(defaultSourceKey);
}

shared_kernel::Result<void> PipeWireAudioGraph::setConfiguredDefaultSource(const std::optional<std::string>& sourceName)
{
    return impl_->setConfiguredDefault(defaultSourceKey, sourceName);
}

void PipeWireAudioGraph::setNoiseSuppressionObserver(NoiseSuppressionObserver* observer)
{
    impl_->setNoiseSuppressionObserver(observer);
}

} // namespace ghub::audio_processing
