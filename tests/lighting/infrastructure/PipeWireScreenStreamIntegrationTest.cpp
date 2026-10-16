// Integração de PipeWireScreenStream contra um daemon pipewire PRIVADO (nunca o da sessão, nunca o portal, nunca a
// tela real). Prova: o fluxo de vídeo só existe entre open() e a destruição; socket inalcançável -> BackendUnavailable.
// O conteúdo dos quadros (portal + compositor) só existe no teste físico; a redução é coberta em ScreenRegionsTest.
#include "modules/lighting/infrastructure/PipeWireScreenStream.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include <poll.h>

#include "PrivatePipeWire.hpp"

namespace ghub::lighting::infrastructure {
namespace {

using audio_processing::testing::PrivatePipeWire;

constexpr const char* kNodeName = "\"node.name\": \"ghub.screen-sampler\"";

bool hasStreamNode(const PrivatePipeWire& daemon)
{
    return daemon.dump().find(kNodeName) != std::string::npos;
}

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

TEST(PipeWireScreenStreamIntegrationTest, UnreachableSocketIsBackendUnavailable)
{
    const auto stream = PipeWireScreenStream::open(ScreenFd{}, 0, [](auto) {}, "/tmp/ghub-pw-inexistente/sock");

    ASSERT_FALSE(stream.has_value());
    EXPECT_EQ(stream.error().code, shared_kernel::ErrorCode::BackendUnavailable);
}

TEST(PipeWireScreenStreamIntegrationTest, OpenCreatesAVideoCaptureNodeAndDestroyingRemovesIt)
{
    PrivatePipeWire daemon;
    EXPECT_FALSE(hasStreamNode(daemon));
    {
        auto stream = PipeWireScreenStream::open(ScreenFd{}, 0, [](auto) {}, daemon.socketPath());

        ASSERT_TRUE(stream.has_value());
        ASSERT_TRUE(eventually([&] { return hasStreamNode(daemon); }));
        EXPECT_NE(daemon.dump().find("\"media.type\": \"Video\""), std::string::npos);
    }
    EXPECT_TRUE(eventually([&] { return !hasStreamNode(daemon); }));
}

} // namespace
} // namespace ghub::lighting::infrastructure
