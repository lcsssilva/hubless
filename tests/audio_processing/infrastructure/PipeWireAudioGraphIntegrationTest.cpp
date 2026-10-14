// Integração de PipeWireAudioGraph contra um daemon pipewire PRIVADO (nunca o da sessão).
#include "modules/audio_processing/infrastructure/PipeWireAudioGraph.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "FakeAlsaDevice.hpp"
#include "PrivatePipeWire.hpp"
#include "TestStreamLink.hpp"

namespace ghub::audio_processing {
namespace {

using testing::FakeAlsaDevice;
using testing::PrivatePipeWire;
using testing::TestStreamLink;

TEST(PipeWireAudioGraphIntegrationTest, ConstructionDoesNotConnectAndMissingSocketIsBackendUnavailable)
{
    PipeWireAudioGraph graph{"/tmp/ghub-pw-inexistente/sock", std::chrono::seconds{2}};
    const auto sink = graph.findHeadsetSink();
    ASSERT_FALSE(sink.has_value());
    EXPECT_EQ(sink.error().code, shared_kernel::ErrorCode::BackendUnavailable);
}

TEST(PipeWireAudioGraphIntegrationTest, WithoutG733OnPrivateDaemonReportsNotFound)
{
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    const auto sink = graph.findHeadsetSink();
    ASSERT_FALSE(sink.has_value());
    EXPECT_EQ(sink.error().code, shared_kernel::ErrorCode::BackendUnavailable);
    EXPECT_EQ(sink.error().detail, "G733 não encontrado");
}

TEST(PipeWireAudioGraphIntegrationTest, FindsG733SinkWhoseVendorAndProductOnlyArriveInTheDeviceInfoEvent)
{
    // Regressão: o evento global do registro NÃO traz device.vendor.id/device.product.id (impl-device.c:30-41@1.6.9);
    // eles só vêm no evento `info` do Device.
    PrivatePipeWire daemon;
    const FakeAlsaDevice other{daemon.socketPath(), "alsa_card.fake-mouse", "0x046d", "0xc08b",
                               "alsa_output.fake-mouse"};
    const FakeAlsaDevice g733{daemon.socketPath(), "alsa_card.fake-g733", "0x046d", "0x0afe", "alsa_output.fake-g733"};
    PipeWireAudioGraph graph{daemon.socketPath()};
    const auto sink = graph.findHeadsetSink();
    ASSERT_TRUE(sink.has_value()) << sink.error().detail;
    EXPECT_EQ(*sink, "alsa_output.fake-g733");
}

TEST(PipeWireAudioGraphIntegrationTest, OtherLogitechDeviceWithAnAlsaSinkIsNotTheG733)
{
    PrivatePipeWire daemon;
    const FakeAlsaDevice other{daemon.socketPath(), "alsa_card.fake-mouse", "0x046d", "0xc08b",
                               "alsa_output.fake-mouse"};
    PipeWireAudioGraph graph{daemon.socketPath()};
    const auto sink = graph.findHeadsetSink();
    ASSERT_FALSE(sink.has_value());
    EXPECT_EQ(sink.error().detail, "G733 não encontrado");
}

bool hasNode(const PrivatePipeWire& daemon, const std::string& name)
{
    return daemon.dump().find("\"node.name\": \"" + name + "\"") != std::string::npos;
}

TEST(PipeWireAudioGraphIntegrationTest, CreateMakesEqualizerSinkAppearAndRemoveMakesItDisappear)
{
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    EXPECT_EQ(graph.equalizerSinkName(), "ghub.g733.eq");
    ASSERT_FALSE(hasNode(daemon, "ghub.g733.eq"));

    const auto created = graph.createEqualizer(EqualizerCurve::flat(), "fake.g733");
    ASSERT_TRUE(created.has_value()) << created.error().detail;
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.eq"));
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.eq.output"));
    EXPECT_NE(daemon.dump().find("Audio/Sink"), std::string::npos);

    const auto again = graph.createEqualizer(EqualizerCurve::flat(), "fake.g733");
    ASSERT_FALSE(again.has_value());
    EXPECT_EQ(again.error().code, shared_kernel::ErrorCode::InvalidArgument);

    ASSERT_TRUE(graph.removeEqualizer().has_value());
    EXPECT_FALSE(hasNode(daemon, "ghub.g733.eq"));
    EXPECT_FALSE(hasNode(daemon, "ghub.g733.eq.output"));
    EXPECT_TRUE(graph.removeEqualizer().has_value()); // idempotente

    // Pode ser recriado depois de removido.
    ASSERT_TRUE(graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.eq"));
}

// Valor do controle `name` nos Props do nó do EQ, lido do daemon privado (pw-dump). nullopt se ausente.
std::optional<double> controlValue(const std::string& dump, const std::string& name)
{
    const std::regex pattern{"\"" + name + "\",\\s*(-?[0-9.]+)"};
    std::smatch match;
    if (!std::regex_search(dump, match, pattern)) {
        return std::nullopt;
    }
    return std::stod(match[1].str());
}

std::string nodeId(const std::string& dump, const std::string& name)
{
    const auto at = dump.find("\"node.name\": \"" + name + "\"");
    if (at == std::string::npos) {
        return {};
    }
    const auto idAt = dump.rfind("\"id\": ", at);
    return dump.substr(idAt, dump.find(',', idAt) - idAt);
}

std::uint32_t nodeIdNumber(const std::string& dump, const std::string& name)
{
    const auto text = nodeId(dump, name); // "\"id\": 42"
    return text.empty() ? 0U : static_cast<std::uint32_t>(std::stoul(text.substr(text.find(':') + 1)));
}

// Espera por condição (o nó sumiu do daemon privado) com prazo de 5 s.
bool waitForNodeGone(const PrivatePipeWire& daemon, const std::string& name)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (std::chrono::steady_clock::now() < end) {
        if (!hasNode(daemon, name)) {
            return true;
        }
    }
    return false;
}

TEST(PipeWireAudioGraphIntegrationTest, UpdateChangesControlsWithoutRecreatingTheNode)
{
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    const auto initial = EqualizerCurve::flat().withBand(0, 12).value();
    ASSERT_TRUE(graph.createEqualizer(initial, "fake.g733").has_value());
    const auto before = daemon.dump();
    EXPECT_EQ(controlValue(before, "eq_1:Gain"), 12.0);
    EXPECT_EQ(controlValue(before, "eq_5:Gain"), 0.0);
    const auto preBefore = controlValue(before, "pre:Gain 1");
    ASSERT_TRUE(preBefore.has_value());
    EXPECT_NEAR(*preBefore, 0.251189, 1e-5);
    const auto idBefore = nodeId(before, "ghub.g733.eq");
    ASSERT_FALSE(idBefore.empty());

    const auto changed = EqualizerCurve::flat().withBand(0, -5).value().withBand(4, 3).value();
    const auto updated = graph.updateEqualizer(changed);
    ASSERT_TRUE(updated.has_value()) << updated.error().detail;

    const auto after = daemon.dump();
    EXPECT_EQ(controlValue(after, "eq_1:Gain"), -5.0);
    EXPECT_EQ(controlValue(after, "eq_5:Gain"), 3.0);
    EXPECT_EQ(controlValue(after, "eq_10:Gain"), 0.0);
    const auto preAfter = controlValue(after, "pre:Gain 1");
    ASSERT_TRUE(preAfter.has_value());
    EXPECT_NEAR(*preAfter, 0.707946, 1e-5); // 10^(-3/20)
    EXPECT_EQ(nodeId(after, "ghub.g733.eq"), idBefore);
}

TEST(PipeWireAudioGraphIntegrationTest, UpdateWithoutEqualizerIsInvalidArgument)
{
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    const auto updated = graph.updateEqualizer(EqualizerCurve::flat());
    ASSERT_FALSE(updated.has_value());
    EXPECT_EQ(updated.error().code, shared_kernel::ErrorCode::InvalidArgument);
}

TEST(PipeWireAudioGraphIntegrationTest, ConfiguredDefaultSinkIsWrittenReadAndErasedInPrivateMetadata)
{
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    const auto initial = graph.configuredDefaultSink();
    ASSERT_TRUE(initial.has_value()) << initial.error().detail;
    EXPECT_FALSE(initial->has_value());

    ASSERT_TRUE(graph.setConfiguredDefaultSink("alsa_output.fake-hdmi").has_value());
    const auto stored = graph.configuredDefaultSink();
    ASSERT_TRUE(stored.has_value());
    ASSERT_TRUE(stored->has_value());
    EXPECT_EQ(**stored, "alsa_output.fake-hdmi");
    const auto dump = daemon.dump();
    EXPECT_NE(dump.find("\"key\": \"default.configured.audio.sink\""), std::string::npos);
    EXPECT_NE(dump.find("alsa_output.fake-hdmi"), std::string::npos);
    EXPECT_NE(dump.find("Spa:String:JSON"), std::string::npos);

    // Outro cliente enxerga o mesmo valor (está no daemon, não no adaptador).
    PipeWireAudioGraph other{daemon.socketPath()};
    const auto seen = other.configuredDefaultSink();
    ASSERT_TRUE(seen.has_value());
    ASSERT_TRUE(seen->has_value());
    EXPECT_EQ(**seen, "alsa_output.fake-hdmi");

    ASSERT_TRUE(graph.setConfiguredDefaultSink("ghub.g733.eq").has_value());
    EXPECT_EQ(**graph.configuredDefaultSink(), "ghub.g733.eq");

    ASSERT_TRUE(graph.setConfiguredDefaultSink(std::nullopt).has_value());
    const auto erased = graph.configuredDefaultSink();
    ASSERT_TRUE(erased.has_value());
    EXPECT_FALSE(erased->has_value());
    EXPECT_EQ(daemon.dump().find("default.configured.audio.sink"), std::string::npos);
}

class RecordingObserver final : public AudioGraphObserver {
public:
    void equalizerLost(const std::string& reason) override
    {
        const std::lock_guard lock{mutex_};
        reasons_.push_back(reason);
        changed_.notify_all();
    }

    // Espera por condição (ao menos `count` notificações) com prazo.
    bool waitForCount(std::size_t count, std::chrono::milliseconds timeout)
    {
        std::unique_lock lock{mutex_};
        return changed_.wait_for(lock, timeout, [&] { return reasons_.size() >= count; });
    }

    std::size_t count()
    {
        const std::lock_guard lock{mutex_};
        return reasons_.size();
    }

private:
    std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<std::string> reasons_;
};

TEST(PipeWireAudioGraphIntegrationTest, OwnRemovalDoesNotNotifyTheObserver)
{
    PrivatePipeWire daemon;
    RecordingObserver observer;
    PipeWireAudioGraph graph{daemon.socketPath()};
    graph.setObserver(&observer);
    ASSERT_TRUE(graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());
    ASSERT_TRUE(graph.removeEqualizer().has_value());
    EXPECT_FALSE(observer.waitForCount(1, std::chrono::milliseconds{300}));
    graph.setObserver(nullptr);
}

TEST(PipeWireAudioGraphIntegrationTest, NodeDestroyedByTheServerNotifiesTheObserverOnce)
{
    PrivatePipeWire daemon;
    RecordingObserver observer;
    PipeWireAudioGraph graph{daemon.socketPath()};
    graph.setObserver(&observer);
    ASSERT_TRUE(graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());

    ASSERT_TRUE(daemon.destroyGlobal(nodeIdNumber(daemon.dump(), "ghub.g733.eq")));
    ASSERT_TRUE(observer.waitForCount(1, std::chrono::seconds{5}));
    EXPECT_EQ(observer.count(), 1U);
    graph.setObserver(nullptr);
    // Depois da perda o sink não existe mais para o adaptador.
    const auto updated = graph.updateEqualizer(EqualizerCurve::flat());
    ASSERT_FALSE(updated.has_value());
    EXPECT_EQ(updated.error().code, shared_kernel::ErrorCode::InvalidArgument);
}

// Observador que troca o próprio observador de dentro do callback (thread do loop): não pode travar.
class SelfDetachingObserver final : public AudioGraphObserver {
public:
    void attach(PipeWireAudioGraph& graph) { graph_ = &graph; }
    void equalizerLost(const std::string&) override
    {
        graph_->setObserver(nullptr);
        const std::lock_guard lock{mutex_};
        ++calls_;
        changed_.notify_all();
    }
    bool waitForCall(std::chrono::milliseconds timeout)
    {
        std::unique_lock lock{mutex_};
        return changed_.wait_for(lock, timeout, [&] { return calls_ > 0; });
    }

private:
    PipeWireAudioGraph* graph_ = nullptr;
    std::mutex mutex_;
    std::condition_variable changed_;
    int calls_ = 0;
};

TEST(PipeWireAudioGraphIntegrationTest, SetObserverFromInsideTheCallbackDoesNotDeadlock)
{
    PrivatePipeWire daemon;
    SelfDetachingObserver observer;
    PipeWireAudioGraph graph{daemon.socketPath()};
    observer.attach(graph);
    graph.setObserver(&observer);
    ASSERT_TRUE(graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());
    daemon.kill();
    ASSERT_TRUE(observer.waitForCall(std::chrono::seconds{5}));
    graph.setObserver(nullptr); // depois do retorno o observador (local ao teste) já não é usado
}

TEST(PipeWireAudioGraphIntegrationTest, DaemonKilledMidwayNotifiesAndNeverBlocks)
{
    PrivatePipeWire daemon;
    RecordingObserver observer;
    PipeWireAudioGraph graph{daemon.socketPath(), std::chrono::seconds{2}};
    graph.setObserver(&observer);
    ASSERT_TRUE(graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());

    daemon.kill();
    ASSERT_TRUE(observer.waitForCount(1, std::chrono::seconds{5}));
    EXPECT_EQ(observer.count(), 1U);

    const auto started = std::chrono::steady_clock::now();
    const auto update = graph.updateEqualizer(EqualizerCurve::flat());
    const auto configured = graph.configuredDefaultSink();
    const auto sink = graph.findHeadsetSink();
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds{6});
    EXPECT_FALSE(update.has_value());
    ASSERT_FALSE(configured.has_value());
    EXPECT_EQ(configured.error().code, shared_kernel::ErrorCode::BackendUnavailable);
    ASSERT_FALSE(sink.has_value());
    EXPECT_EQ(sink.error().code, shared_kernel::ErrorCode::BackendUnavailable);
    EXPECT_TRUE(graph.removeEqualizer().has_value());
    graph.setObserver(nullptr);
}

TEST(PipeWireAudioGraphIntegrationTest, DestroyingTheGraphRemovesTheEqualizerNode)
{
    PrivatePipeWire daemon;
    {
        PipeWireAudioGraph graph{daemon.socketPath()};
        ASSERT_TRUE(graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());
        ASSERT_TRUE(hasNode(daemon, "ghub.g733.eq"));
    }
    EXPECT_TRUE(waitForNodeGone(daemon, "ghub.g733.eq"));
}

TEST(PipeWireAudioGraphIntegrationTest, KilledClientProcessLeavesNoEqualizerNodeBehind)
{
    PrivatePipeWire daemon;
    int ready[2];
    ASSERT_EQ(pipe(ready), 0);
    const pid_t child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        PipeWireAudioGraph graph{daemon.socketPath()};
        const bool ok = graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value();
        const char byte = ok ? 'k' : 'f';
        (void)!write(ready[1], &byte, 1);
        pause(); // fica vivo até receber SIGKILL: sem destrutores, como uma queda do app
        _exit(0);
    }
    close(ready[1]);
    char byte = 0;
    ASSERT_EQ(read(ready[0], &byte, 1), 1);
    close(ready[0]);
    ASSERT_EQ(byte, 'k');
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.eq"));
    kill(child, SIGKILL);
    int status = 0;
    waitpid(child, &status, 0);
    EXPECT_TRUE(waitForNodeGone(daemon, "ghub.g733.eq"));
}

// Fluxo de aplicativo ligado ao sink do EQ: remover o EQ espera o WirePlumber mover o fluxo (senão o app, como o
// Spotify, pausa quando a saída some).
TEST(PipeWireAudioGraphIntegrationTest, RemoveWaitsUntilNoLinkFeedsTheEqualizer)
{
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    ASSERT_TRUE(graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());
    TestStreamLink stream{daemon.socketPath()};
    stream.link(nodeIdNumber(daemon.dump(), "ghub.g733.eq"));

    auto removal = std::async(std::launch::async, [&graph] { return graph.removeEqualizer(); });
    // Enquanto o link existe, a remoção não termina e o nó continua no daemon (observação limitada, não sincronização).
    EXPECT_EQ(removal.wait_for(std::chrono::milliseconds{500}), std::future_status::timeout);
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.eq"));

    stream.unlink(); // o "WirePlumber" move o fluxo: o link some
    ASSERT_EQ(removal.wait_for(std::chrono::seconds{1}), std::future_status::ready);
    EXPECT_TRUE(removal.get().has_value());
    EXPECT_TRUE(waitForNodeGone(daemon, "ghub.g733.eq"));
}

TEST(PipeWireAudioGraphIntegrationTest, RemoveDestroysAnywayAfterTheShortDeadlineWhenALinkIsPinned)
{
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    ASSERT_TRUE(graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());
    TestStreamLink stream{daemon.socketPath()};
    stream.link(nodeIdNumber(daemon.dump(), "ghub.g733.eq"));

    const auto started = std::chrono::steady_clock::now();
    EXPECT_TRUE(graph.removeEqualizer().has_value());
    const auto elapsed = std::chrono::steady_clock::now() - started;
    EXPECT_GE(elapsed, std::chrono::milliseconds{1900});
    EXPECT_LT(elapsed, std::chrono::seconds{4});
    EXPECT_TRUE(waitForNodeGone(daemon, "ghub.g733.eq"));
}

TEST(PipeWireAudioGraphIntegrationTest, DestroyingTheGraphAlsoWaitsForTheLink)
{
    PrivatePipeWire daemon;
    auto graph = std::make_unique<PipeWireAudioGraph>(daemon.socketPath());
    ASSERT_TRUE(graph->createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());
    TestStreamLink stream{daemon.socketPath()};
    stream.link(nodeIdNumber(daemon.dump(), "ghub.g733.eq"));

    auto destruction = std::async(std::launch::async, [&graph] { graph.reset(); });
    EXPECT_EQ(destruction.wait_for(std::chrono::milliseconds{500}), std::future_status::timeout);
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.eq"));
    stream.unlink();
    ASSERT_EQ(destruction.wait_for(std::chrono::seconds{1}), std::future_status::ready);
    EXPECT_TRUE(waitForNodeGone(daemon, "ghub.g733.eq"));
}

TEST(PipeWireAudioGraphIntegrationTest, SecondInstanceCannotCreateAnotherEqualizerWithTheSameName)
{
    PrivatePipeWire daemon;
    PipeWireAudioGraph first{daemon.socketPath()};
    PipeWireAudioGraph second{daemon.socketPath()};
    ASSERT_TRUE(first.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());
    const auto duplicate = second.createEqualizer(EqualizerCurve::flat(), "fake.g733");
    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(duplicate.error().code, shared_kernel::ErrorCode::DeviceBusy);
    EXPECT_TRUE(second.removeEqualizer().has_value()); // não derruba o EQ da outra instância
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.eq"));
}

TEST(PipeWireAudioGraphIntegrationTest, UnresponsiveServerHitsTheDeadlineAsBackendUnavailable)
{
    // "Daemon" que aceita conexões (backlog do kernel) e nunca responde.
    char pattern[] = "/tmp/ghub-pw-mudo-XXXXXX";
    ASSERT_NE(mkdtemp(pattern), nullptr);
    const std::string dir = pattern;
    const std::string path = dir + "/sock";
    const int server = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    path.copy(addr.sun_path, sizeof addr.sun_path - 1);
    ASSERT_EQ(bind(server, reinterpret_cast<const sockaddr*>(&addr), sizeof addr), 0);
    ASSERT_EQ(listen(server, 4), 0);

    {
        PipeWireAudioGraph graph{path, std::chrono::milliseconds{800}};
        const auto started = std::chrono::steady_clock::now();
        const auto sink = graph.findHeadsetSink();
        const auto elapsed = std::chrono::steady_clock::now() - started;
        ASSERT_FALSE(sink.has_value());
        EXPECT_EQ(sink.error().code, shared_kernel::ErrorCode::BackendUnavailable);
        EXPECT_GE(elapsed, std::chrono::milliseconds{700});
        EXPECT_LT(elapsed, std::chrono::seconds{4});
        const auto created = graph.createEqualizer(EqualizerCurve::flat(), "fake.g733");
        ASSERT_FALSE(created.has_value());
        EXPECT_EQ(created.error().code, shared_kernel::ErrorCode::BackendUnavailable);
    }
    close(server);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

} // namespace
} // namespace ghub::audio_processing

int main(int argc, char** argv)
{
    // Defesa em profundidade: qualquer conexão por padrão aponta para um diretório vazio, nunca para a sessão.
    char pattern[] = "/tmp/ghub-pw-env-XXXXXX";
    if (mkdtemp(pattern) == nullptr) {
        return 1;
    }
    setenv("XDG_RUNTIME_DIR", pattern, 1);
    setenv("PIPEWIRE_RUNTIME_DIR", pattern, 1);
    unsetenv("PIPEWIRE_REMOTE");
    unsetenv("PIPEWIRE_CORE");
    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    std::error_code ec;
    std::filesystem::remove_all(pattern, ec);
    return result;
}
