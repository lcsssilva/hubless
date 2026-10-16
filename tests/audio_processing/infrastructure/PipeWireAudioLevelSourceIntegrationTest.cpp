// Integração de PipeWireAudioLevelSource contra um daemon pipewire PRIVADO (nunca o da sessão, nunca áudio real).
// Prova: nada é aberto antes de start(); start() cria um fluxo de captura do monitor do sink padrão; stop() e o
// destrutor o removem; socket inalcançável -> BackendUnavailable. O fluxo de amostras reais (WirePlumber ligando o
// monitor ao fluxo) não existe no daemon privado e fica para o teste físico.
#include "modules/audio_processing/infrastructure/PipeWireAudioLevelSource.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <poll.h>
#include <string>

#include "PrivatePipeWire.hpp"

namespace ghub::audio_processing {
namespace {

using testing::PrivatePipeWire;

constexpr const char* kNodeName = "\"node.name\": \"ghub.audio-visualizer\"";

bool hasCaptureNode(const PrivatePipeWire& daemon)
{
    return daemon.dump().find(kNodeName) != std::string::npos;
}

// Espera por condição com prazo (o registro do daemon demora alguns ms para refletir a criação/remoção).
template <typename Predicate> bool eventually(Predicate&& predicate)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (std::chrono::steady_clock::now() < end) {
        if (predicate()) {
            return true;
        }
        poll(nullptr, 0, 50); // intervalo entre tentativas de uma espera com prazo por condição observável
    }
    return predicate();
}

TEST(PipeWireAudioLevelSourceIntegrationTest, ConstructionOpensNothing)
{
    PrivatePipeWire daemon;
    const PipeWireAudioLevelSource source{daemon.socketPath()};

    EXPECT_FALSE(source.running());
    EXPECT_FALSE(hasCaptureNode(daemon));
    for (const float level : source.levels()) {
        EXPECT_EQ(level, 0.0F);
    }
}

TEST(PipeWireAudioLevelSourceIntegrationTest, UnreachableSocketIsBackendUnavailableAndNothingStaysOpen)
{
    PipeWireAudioLevelSource source{"/tmp/ghub-pw-inexistente/sock"};

    const auto started = source.start();

    ASSERT_FALSE(started.has_value());
    EXPECT_EQ(started.error().code, shared_kernel::ErrorCode::BackendUnavailable);
    EXPECT_FALSE(source.running());
}

TEST(PipeWireAudioLevelSourceIntegrationTest, StartCreatesACaptureStreamOfTheSinkMonitorAndStopRemovesIt)
{
    PrivatePipeWire daemon;
    PipeWireAudioLevelSource source{daemon.socketPath()};

    ASSERT_TRUE(source.start().has_value());
    EXPECT_TRUE(source.running());
    ASSERT_TRUE(eventually([&] { return hasCaptureNode(daemon); }));
    EXPECT_NE(daemon.dump().find("\"stream.capture.sink\": true"),
              std::string::npos); // pw-dump converte "true" em booleano

    source.stop();

    EXPECT_FALSE(source.running());
    EXPECT_TRUE(eventually([&] { return !hasCaptureNode(daemon); }));
    for (const float level : source.levels()) {
        EXPECT_EQ(level, 0.0F);
    }
}

TEST(PipeWireAudioLevelSourceIntegrationTest, StartTwiceCreatesASingleStreamAndStopIsIdempotent)
{
    PrivatePipeWire daemon;
    PipeWireAudioLevelSource source{daemon.socketPath()};

    ASSERT_TRUE(source.start().has_value());
    ASSERT_TRUE(source.start().has_value());
    ASSERT_TRUE(eventually([&] { return hasCaptureNode(daemon); }));
    const auto dump = daemon.dump();
    const auto first = dump.find(kNodeName);
    EXPECT_EQ(dump.find(kNodeName, first + 1), std::string::npos);

    source.stop();
    source.stop();
    EXPECT_TRUE(eventually([&] { return !hasCaptureNode(daemon); }));
}

TEST(PipeWireAudioLevelSourceIntegrationTest, CanBeRestartedAfterStop)
{
    PrivatePipeWire daemon;
    PipeWireAudioLevelSource source{daemon.socketPath()};

    ASSERT_TRUE(source.start().has_value());
    source.stop();
    ASSERT_TRUE(source.start().has_value());
    EXPECT_TRUE(eventually([&] { return hasCaptureNode(daemon); }));
}

TEST(PipeWireAudioLevelSourceIntegrationTest, DestroyingWhileRunningRemovesTheStream)
{
    PrivatePipeWire daemon;
    {
        PipeWireAudioLevelSource source{daemon.socketPath()};
        ASSERT_TRUE(source.start().has_value());
        ASSERT_TRUE(eventually([&] { return hasCaptureNode(daemon); }));
    }
    EXPECT_TRUE(eventually([&] { return !hasCaptureNode(daemon); }));
}

} // namespace
} // namespace ghub::audio_processing
