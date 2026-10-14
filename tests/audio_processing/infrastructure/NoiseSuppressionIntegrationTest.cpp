// Integração da redução de ruído do microfone (PipeWireAudioGraph como NoiseSuppressionGraph) contra um daemon
// pipewire PRIVADO (nunca o da sessão). O plugin RNNoise é carregado dentro do processo do teste.
#include "modules/audio_processing/infrastructure/PipeWireAudioGraph.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include "FakeAlsaDevice.hpp"
#include "PrivatePipeWire.hpp"
#include "TestStreamLink.hpp"

namespace ghub::audio_processing {
namespace {

using testing::FakeAlsaDevice;
using testing::PrivatePipeWire;
using testing::TestStreamLink;

constexpr const char* pluginPath = "/usr/lib/ladspa/librnnoise_ladspa.so";
constexpr const char* thresholdControl = "rnnoise:VAD Threshold (%)";

bool pluginInstalled()
{
    return std::filesystem::exists(pluginPath);
}

#define SKIP_WITHOUT_PLUGIN()                                                                                          \
    if (!pluginInstalled()) {                                                                                          \
        GTEST_SKIP() << "plugin RNNoise ausente (" << pluginPath << "): pacote noise-suppression-for-voice";           \
    }

// Troca LADSPA_PATH no processo do teste (o plugin é carregado aqui) e restaura ao sair.
class ScopedLadspaPath {
public:
    explicit ScopedLadspaPath(const std::string& value)
    {
        if (const char* old = std::getenv("LADSPA_PATH")) {
            previous_ = old;
        }
        setenv("LADSPA_PATH", value.c_str(), 1);
    }
    ~ScopedLadspaPath()
    {
        if (previous_) {
            setenv("LADSPA_PATH", previous_->c_str(), 1);
        } else {
            unsetenv("LADSPA_PATH");
        }
    }
    ScopedLadspaPath(const ScopedLadspaPath&) = delete;
    ScopedLadspaPath& operator=(const ScopedLadspaPath&) = delete;
    ScopedLadspaPath(ScopedLadspaPath&&) = delete;
    ScopedLadspaPath& operator=(ScopedLadspaPath&&) = delete;

private:
    std::optional<std::string> previous_;
};

bool hasNode(const PrivatePipeWire& daemon, const std::string& name)
{
    return daemon.dump().find("\"node.name\": \"" + name + "\"") != std::string::npos;
}

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

std::uint32_t nodeIdNumber(const std::string& dump, const std::string& name)
{
    const auto at = dump.find("\"node.name\": \"" + name + "\"");
    if (at == std::string::npos) {
        return 0;
    }
    const auto idAt = dump.rfind("\"id\": ", at);
    return static_cast<std::uint32_t>(std::stoul(dump.substr(idAt + 6, dump.find(',', idAt) - idAt - 6)));
}

std::string escapeRegex(const std::string& text)
{
    static const std::regex special{R"([.^$|()\[\]{}*+?\\])"};
    return std::regex_replace(text, special, R"(\$&)");
}

std::optional<double> controlValue(const std::string& dump, const std::string& name)
{
    const std::regex pattern{"\"" + escapeRegex(name) + "\",\\s*(-?[0-9.]+)"};
    std::smatch match;
    if (!std::regex_search(dump, match, pattern)) {
        return std::nullopt;
    }
    return std::stod(match[1].str());
}

VoiceThreshold threshold(int percent)
{
    return VoiceThreshold::create(percent).value();
}

class RecordingNoiseObserver final : public NoiseSuppressionObserver {
public:
    void noiseSuppressionLost(const std::string& reason) override
    {
        const std::lock_guard lock{mutex_};
        reasons_.push_back(reason);
        changed_.notify_all();
    }
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

class RecordingEqObserver final : public AudioGraphObserver {
public:
    void equalizerLost(const std::string& reason) override
    {
        const std::lock_guard lock{mutex_};
        reasons_.push_back(reason);
        changed_.notify_all();
    }
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

TEST(NoiseSuppressionIntegrationTest, FindsG733SourceWhoseVendorAndProductOnlyArriveInTheDeviceInfoEvent)
{
    PrivatePipeWire daemon;
    const FakeAlsaDevice other{daemon.socketPath(),     "alsa_card.fake-mouse", "0x046d", "0xc08b",
                               "alsa_input.fake-mouse", "Audio/Source"};
    const FakeAlsaDevice g733{daemon.socketPath(),    "alsa_card.fake-g733", "0x046d", "0x0afe",
                              "alsa_input.fake-g733", "Audio/Source"};
    PipeWireAudioGraph graph{daemon.socketPath()};
    const auto source = graph.findHeadsetSource();
    ASSERT_TRUE(source.has_value()) << source.error().detail;
    EXPECT_EQ(*source, "alsa_input.fake-g733");
}

TEST(NoiseSuppressionIntegrationTest, G733WithOnlyASinkHasNoSource)
{
    PrivatePipeWire daemon;
    const FakeAlsaDevice g733{daemon.socketPath(), "alsa_card.fake-g733", "0x046d", "0x0afe", "alsa_output.fake-g733"};
    PipeWireAudioGraph graph{daemon.socketPath()};
    const auto source = graph.findHeadsetSource();
    ASSERT_FALSE(source.has_value());
    EXPECT_EQ(source.error().code, shared_kernel::ErrorCode::BackendUnavailable);
    EXPECT_EQ(source.error().detail, "G733 não encontrado");
}

TEST(NoiseSuppressionIntegrationTest, MissingPluginIsBackendUnavailableAndLeavesNothingCreated)
{
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    const auto emptyDir = std::filesystem::path{daemon.directory()} / "ladspa-vazio";
    std::filesystem::create_directories(emptyDir);
    {
        const ScopedLadspaPath path{emptyDir.string()};
        const auto created = graph.createNoiseSuppression(threshold(50), "fake.mic");
        ASSERT_FALSE(created.has_value());
        EXPECT_EQ(created.error().code, shared_kernel::ErrorCode::BackendUnavailable);
        EXPECT_EQ(created.error().detail, "plugin de redução de ruído não instalado (noise-suppression-for-voice)");
    }
    EXPECT_FALSE(hasNode(daemon, "ghub.g733.mic"));
    EXPECT_FALSE(hasNode(daemon, "ghub.g733.mic.input"));
    // Nada ficou preso no adaptador: sem fonte, remover é sucesso e atualizar é argumento inválido.
    EXPECT_TRUE(graph.removeNoiseSuppression().has_value());
    const auto updated = graph.updateNoiseSuppression(threshold(60));
    ASSERT_FALSE(updated.has_value());
    EXPECT_EQ(updated.error().code, shared_kernel::ErrorCode::InvalidArgument);
}

TEST(NoiseSuppressionIntegrationTest, CreateMakesAudioSourceAppearAndRemoveMakesItDisappear)
{
    SKIP_WITHOUT_PLUGIN();
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    EXPECT_EQ(graph.noiseSuppressionSourceName(), "ghub.g733.mic");
    ASSERT_FALSE(hasNode(daemon, "ghub.g733.mic"));

    const auto created = graph.createNoiseSuppression(threshold(50), "fake.mic");
    ASSERT_TRUE(created.has_value()) << created.error().detail;
    const auto dump = daemon.dump();
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.mic"));
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.mic.input"));
    const auto at = dump.find("\"node.name\": \"ghub.g733.mic\"");
    ASSERT_NE(at, std::string::npos);
    const auto objectStart = dump.rfind("\"id\": ", at);
    const auto objectEnd = dump.find("\"node.name\"", at + 10);
    EXPECT_NE(dump.substr(objectStart, objectEnd - objectStart).find("\"media.class\": \"Audio/Source\""),
              std::string::npos);

    const auto again = graph.createNoiseSuppression(threshold(50), "fake.mic");
    ASSERT_FALSE(again.has_value());
    EXPECT_EQ(again.error().code, shared_kernel::ErrorCode::InvalidArgument);

    ASSERT_TRUE(graph.removeNoiseSuppression().has_value());
    EXPECT_FALSE(hasNode(daemon, "ghub.g733.mic"));
    EXPECT_FALSE(hasNode(daemon, "ghub.g733.mic.input"));
    EXPECT_TRUE(graph.removeNoiseSuppression().has_value()); // idempotente

    ASSERT_TRUE(graph.createNoiseSuppression(threshold(50), "fake.mic").has_value()); // recriável
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.mic"));
}

TEST(NoiseSuppressionIntegrationTest, UpdateChangesTheThresholdWithoutRecreatingTheNode)
{
    SKIP_WITHOUT_PLUGIN();
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    ASSERT_TRUE(graph.createNoiseSuppression(threshold(50), "fake.mic").has_value());
    const auto before = daemon.dump();
    EXPECT_EQ(controlValue(before, thresholdControl), 50.0);
    const auto idBefore = nodeIdNumber(before, "ghub.g733.mic");
    ASSERT_NE(idBefore, 0U);

    const auto updated = graph.updateNoiseSuppression(threshold(73));
    ASSERT_TRUE(updated.has_value()) << updated.error().detail;
    const auto after = daemon.dump();
    EXPECT_EQ(controlValue(after, thresholdControl), 73.0);
    EXPECT_EQ(nodeIdNumber(after, "ghub.g733.mic"), idBefore);
}

TEST(NoiseSuppressionIntegrationTest, UpdateWithoutSourceIsInvalidArgument)
{
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    const auto updated = graph.updateNoiseSuppression(threshold(50));
    ASSERT_FALSE(updated.has_value());
    EXPECT_EQ(updated.error().code, shared_kernel::ErrorCode::InvalidArgument);
}

TEST(NoiseSuppressionIntegrationTest, ConfiguredDefaultSourceIsWrittenReadAndErasedWithoutTouchingTheSink)
{
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    const auto initial = graph.configuredDefaultSource();
    ASSERT_TRUE(initial.has_value()) << initial.error().detail;
    EXPECT_FALSE(initial->has_value());

    ASSERT_TRUE(graph.setConfiguredDefaultSink("alsa_output.fake").has_value());
    ASSERT_TRUE(graph.setConfiguredDefaultSource("alsa_input.fake-g733").has_value());
    const auto stored = graph.configuredDefaultSource();
    ASSERT_TRUE(stored.has_value());
    ASSERT_TRUE(stored->has_value());
    EXPECT_EQ(**stored, "alsa_input.fake-g733");
    EXPECT_EQ(**graph.configuredDefaultSink(), "alsa_output.fake");
    const auto dump = daemon.dump();
    EXPECT_NE(dump.find("\"key\": \"default.configured.audio.source\""), std::string::npos);
    EXPECT_NE(dump.find("Spa:String:JSON"), std::string::npos);

    PipeWireAudioGraph other{daemon.socketPath()};
    const auto seen = other.configuredDefaultSource();
    ASSERT_TRUE(seen.has_value());
    ASSERT_TRUE(seen->has_value());
    EXPECT_EQ(**seen, "alsa_input.fake-g733");

    ASSERT_TRUE(graph.setConfiguredDefaultSource(std::nullopt).has_value());
    const auto erased = graph.configuredDefaultSource();
    ASSERT_TRUE(erased.has_value());
    EXPECT_FALSE(erased->has_value());
    EXPECT_EQ(daemon.dump().find("default.configured.audio.source"), std::string::npos);
    EXPECT_EQ(**graph.configuredDefaultSink(), "alsa_output.fake"); // a saída segue intacta
}

TEST(NoiseSuppressionIntegrationTest, OwnRemovalDoesNotNotifyTheObserver)
{
    SKIP_WITHOUT_PLUGIN();
    PrivatePipeWire daemon;
    RecordingNoiseObserver observer;
    PipeWireAudioGraph graph{daemon.socketPath()};
    graph.setNoiseSuppressionObserver(&observer);
    ASSERT_TRUE(graph.createNoiseSuppression(threshold(50), "fake.mic").has_value());
    ASSERT_TRUE(graph.removeNoiseSuppression().has_value());
    EXPECT_FALSE(observer.waitForCount(1, std::chrono::milliseconds{300}));
    graph.setNoiseSuppressionObserver(nullptr);
}

TEST(NoiseSuppressionIntegrationTest, NodeDestroyedByTheServerNotifiesOnlyTheNoiseObserverOnce)
{
    SKIP_WITHOUT_PLUGIN();
    PrivatePipeWire daemon;
    RecordingNoiseObserver noise;
    RecordingEqObserver eq;
    PipeWireAudioGraph graph{daemon.socketPath()};
    graph.setNoiseSuppressionObserver(&noise);
    graph.setObserver(&eq);
    ASSERT_TRUE(graph.createNoiseSuppression(threshold(50), "fake.mic").has_value());

    ASSERT_TRUE(daemon.destroyGlobal(nodeIdNumber(daemon.dump(), "ghub.g733.mic")));
    ASSERT_TRUE(noise.waitForCount(1, std::chrono::seconds{5}));
    EXPECT_EQ(noise.count(), 1U);
    EXPECT_FALSE(eq.waitForCount(1, std::chrono::milliseconds{300}));
    graph.setNoiseSuppressionObserver(nullptr);
    graph.setObserver(nullptr);
    const auto updated = graph.updateNoiseSuppression(threshold(50));
    ASSERT_FALSE(updated.has_value());
    EXPECT_EQ(updated.error().code, shared_kernel::ErrorCode::InvalidArgument);
}

TEST(NoiseSuppressionIntegrationTest, DaemonKilledMidwayNotifiesBothInstancesIndependently)
{
    SKIP_WITHOUT_PLUGIN();
    PrivatePipeWire daemon;
    RecordingNoiseObserver noise;
    RecordingEqObserver eq;
    PipeWireAudioGraph graph{daemon.socketPath(), std::chrono::seconds{2}};
    graph.setNoiseSuppressionObserver(&noise);
    graph.setObserver(&eq);
    ASSERT_TRUE(graph.createNoiseSuppression(threshold(50), "fake.mic").has_value());
    ASSERT_TRUE(graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());

    daemon.kill();
    ASSERT_TRUE(noise.waitForCount(1, std::chrono::seconds{5}));
    ASSERT_TRUE(eq.waitForCount(1, std::chrono::seconds{5}));
    EXPECT_EQ(noise.count(), 1U);
    EXPECT_EQ(eq.count(), 1U);
    EXPECT_TRUE(graph.removeNoiseSuppression().has_value());
    EXPECT_TRUE(graph.removeEqualizer().has_value());
    graph.setNoiseSuppressionObserver(nullptr);
    graph.setObserver(nullptr);
}

TEST(NoiseSuppressionIntegrationTest, EqualizerAndMicrophoneLiveTogetherAndRemoveIndependently)
{
    SKIP_WITHOUT_PLUGIN();
    PrivatePipeWire daemon;
    RecordingNoiseObserver noise;
    RecordingEqObserver eq;
    PipeWireAudioGraph graph{daemon.socketPath()};
    graph.setNoiseSuppressionObserver(&noise);
    graph.setObserver(&eq);
    ASSERT_TRUE(graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());
    ASSERT_TRUE(graph.createNoiseSuppression(threshold(50), "fake.mic").has_value());
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.eq"));
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.mic"));

    ASSERT_TRUE(graph.removeNoiseSuppression().has_value());
    EXPECT_FALSE(hasNode(daemon, "ghub.g733.mic"));
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.eq"));
    ASSERT_TRUE(graph.updateEqualizer(EqualizerCurve::flat().withBand(0, 3).value()).has_value());

    ASSERT_TRUE(graph.createNoiseSuppression(threshold(40), "fake.mic").has_value());
    ASSERT_TRUE(graph.removeEqualizer().has_value());
    EXPECT_FALSE(hasNode(daemon, "ghub.g733.eq"));
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.mic"));
    ASSERT_TRUE(graph.updateNoiseSuppression(threshold(41)).has_value());
    EXPECT_EQ(controlValue(daemon.dump(), thresholdControl), 41.0);

    // Perda de um não notifica o outro.
    ASSERT_TRUE(graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());
    ASSERT_TRUE(daemon.destroyGlobal(nodeIdNumber(daemon.dump(), "ghub.g733.eq")));
    ASSERT_TRUE(eq.waitForCount(1, std::chrono::seconds{5}));
    EXPECT_EQ(noise.count(), 0U);
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.mic"));
    graph.setNoiseSuppressionObserver(nullptr);
    graph.setObserver(nullptr);
}

TEST(NoiseSuppressionIntegrationTest, RemoveWaitsUntilNoLinkLeavesTheFilteredSource)
{
    SKIP_WITHOUT_PLUGIN();
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    ASSERT_TRUE(graph.createNoiseSuppression(threshold(50), "fake.mic").has_value());
    TestStreamLink recorder{daemon.socketPath()};
    recorder.linkFromOutput(nodeIdNumber(daemon.dump(), "ghub.g733.mic"));

    auto removal = std::async(std::launch::async, [&graph] { return graph.removeNoiseSuppression(); });
    EXPECT_EQ(removal.wait_for(std::chrono::milliseconds{500}), std::future_status::timeout);
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.mic"));

    recorder.unlink(); // o "WirePlumber" move o fluxo: o link some
    ASSERT_EQ(removal.wait_for(std::chrono::seconds{1}), std::future_status::ready);
    EXPECT_TRUE(removal.get().has_value());
    EXPECT_TRUE(waitForNodeGone(daemon, "ghub.g733.mic"));
}

TEST(NoiseSuppressionIntegrationTest, RemoveDestroysAnywayAfterTheShortDeadlineWhenALinkIsPinned)
{
    SKIP_WITHOUT_PLUGIN();
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    ASSERT_TRUE(graph.createNoiseSuppression(threshold(50), "fake.mic").has_value());
    TestStreamLink recorder{daemon.socketPath()};
    recorder.linkFromOutput(nodeIdNumber(daemon.dump(), "ghub.g733.mic"));

    const auto started = std::chrono::steady_clock::now();
    EXPECT_TRUE(graph.removeNoiseSuppression().has_value());
    const auto elapsed = std::chrono::steady_clock::now() - started;
    EXPECT_GE(elapsed, std::chrono::milliseconds{1900});
    EXPECT_LT(elapsed, std::chrono::seconds{4});
    EXPECT_TRUE(waitForNodeGone(daemon, "ghub.g733.mic"));
}

TEST(NoiseSuppressionIntegrationTest, LinkIntoTheMicrophoneDoesNotDelayRemovalOfTheSource)
{
    // A regra da fonte olha links que SAEM de ghub.g733.mic; links que ENTRAM em outro nó não contam.
    SKIP_WITHOUT_PLUGIN();
    PrivatePipeWire daemon;
    PipeWireAudioGraph graph{daemon.socketPath()};
    ASSERT_TRUE(graph.createNoiseSuppression(threshold(50), "fake.mic").has_value());
    ASSERT_TRUE(graph.createEqualizer(EqualizerCurve::flat(), "fake.g733").has_value());
    TestStreamLink stream{daemon.socketPath()};
    stream.link(nodeIdNumber(daemon.dump(), "ghub.g733.eq")); // fluxo entra no EQ, não toca o microfone

    const auto started = std::chrono::steady_clock::now();
    EXPECT_TRUE(graph.removeNoiseSuppression().has_value());
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::milliseconds{1500});
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.eq"));
}

TEST(NoiseSuppressionIntegrationTest, DestroyingTheGraphRemovesTheMicrophoneNode)
{
    SKIP_WITHOUT_PLUGIN();
    PrivatePipeWire daemon;
    {
        PipeWireAudioGraph graph{daemon.socketPath()};
        ASSERT_TRUE(graph.createNoiseSuppression(threshold(50), "fake.mic").has_value());
        ASSERT_TRUE(hasNode(daemon, "ghub.g733.mic"));
    }
    EXPECT_TRUE(waitForNodeGone(daemon, "ghub.g733.mic"));
}

TEST(NoiseSuppressionIntegrationTest, SecondInstanceCannotCreateAnotherMicrophoneWithTheSameName)
{
    SKIP_WITHOUT_PLUGIN();
    PrivatePipeWire daemon;
    PipeWireAudioGraph first{daemon.socketPath()};
    PipeWireAudioGraph second{daemon.socketPath()};
    ASSERT_TRUE(first.createNoiseSuppression(threshold(50), "fake.mic").has_value());
    const auto duplicate = second.createNoiseSuppression(threshold(50), "fake.mic");
    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(duplicate.error().code, shared_kernel::ErrorCode::DeviceBusy);
    EXPECT_TRUE(second.removeNoiseSuppression().has_value());
    EXPECT_TRUE(hasNode(daemon, "ghub.g733.mic"));
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
