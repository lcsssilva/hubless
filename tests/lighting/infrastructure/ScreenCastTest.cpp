// Sequência do portal ScreenCast e fonte de cores da tela, só com fakes: o portal real e a tela real nunca são tocados.
#include "modules/lighting/infrastructure/PortalScreenColorSource.hpp"

#include "fakes/FakeScreenCastPortal.hpp"
#include "modules/lighting/infrastructure/ScreenCastPortal.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using ghub::fakes::FakeScreenCastPortal;
using ghub::lighting::domain::RgbColor;
using ghub::lighting::infrastructure::PortalScreenColorSource;
using ghub::lighting::infrastructure::RegionsCallback;
using ghub::lighting::infrastructure::requestScreenCast;
using ghub::lighting::infrastructure::ScreenFd;
using ghub::lighting::infrastructure::ScreenStream;
using ghub::shared_kernel::ErrorCode;
using Calls = std::vector<std::string>;

TEST(RequestScreenCastTest, RunsTheFourPortalCallsInOrderAndClosesTheSessionOnDestruction)
{
    FakeScreenCastPortal portal;
    const std::atomic<bool> cancelled{false};
    {
        auto grant = requestScreenCast(portal, cancelled);

        ASSERT_TRUE(grant.has_value());
        EXPECT_EQ(grant->nodeId(), 42U);
        EXPECT_TRUE(grant->takeFd().valid());
        EXPECT_EQ(portal.calls(), (Calls{"CreateSession", "SelectSources:/session/fake", "Start:/session/fake",
                                         "OpenPipeWireRemote:/session/fake"}));
    }
    EXPECT_EQ(portal.calls().back(), "Close:/session/fake");
    EXPECT_EQ(portal.calls().size(), 5U);
}

TEST(RequestScreenCastTest, MovingTheGrantClosesTheSessionOnlyOnce)
{
    FakeScreenCastPortal portal;
    const std::atomic<bool> cancelled{false};
    {
        auto grant = requestScreenCast(portal, cancelled);
        ASSERT_TRUE(grant.has_value());
        auto moved = *std::move(grant);
    }
    EXPECT_EQ(std::ranges::count(portal.calls(), "Close:/session/fake"), 1);
}

TEST(RequestScreenCastTest, FailureAtCreateSessionStopsWithoutClosingAnything)
{
    FakeScreenCastPortal portal;
    portal.failAt("CreateSession");
    const std::atomic<bool> cancelled{false};

    const auto grant = requestScreenCast(portal, cancelled);

    ASSERT_FALSE(grant.has_value());
    EXPECT_EQ(grant.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(portal.calls(), (Calls{"CreateSession"}));
}

TEST(RequestScreenCastTest, FailureAfterTheSessionExistsClosesIt)
{
    for (const auto* step : {"SelectSources", "Start", "OpenPipeWireRemote"}) {
        FakeScreenCastPortal portal;
        portal.failAt(step);
        const std::atomic<bool> cancelled{false};

        const auto grant = requestScreenCast(portal, cancelled);

        ASSERT_FALSE(grant.has_value()) << step;
        EXPECT_EQ(portal.calls().back(), "Close:/session/fake") << step;
    }
}

TEST(RequestScreenCastTest, DoesNotAskForTheRemoteWhenTheUserRefused)
{
    FakeScreenCastPortal portal;
    portal.failAt("Start");
    const std::atomic<bool> cancelled{false};

    ASSERT_FALSE(requestScreenCast(portal, cancelled).has_value());

    EXPECT_EQ(std::ranges::count(portal.calls(), "OpenPipeWireRemote:/session/fake"), 0);
}

// Fluxo falso: guarda o callback e o destrutor avisa o teste.
class FakeStream final : public ScreenStream {
public:
    explicit FakeStream(std::atomic<int>& alive) : alive_(&alive) { ++*alive_; }
    FakeStream(const FakeStream&) = delete;
    FakeStream& operator=(const FakeStream&) = delete;
    FakeStream(FakeStream&&) = delete;
    FakeStream& operator=(FakeStream&&) = delete;
    ~FakeStream() override { --*alive_; }

private:
    std::atomic<int>* alive_;
};

class PortalScreenColorSourceTest : public ::testing::Test {
protected:
    PortalScreenColorSource make()
    {
        return PortalScreenColorSource{
            portal, [this](ScreenFd fd, std::uint32_t node, RegionsCallback callback) {
                fdValid = fd.valid();
                nodeId = node;
                onRegions = std::move(callback);
                ++factoryCalls;
                if (failFactory) {
                    return ghub::shared_kernel::Result<std::unique_ptr<ScreenStream>>{
                        ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "sem PipeWire")};
                }
                return ghub::shared_kernel::Result<std::unique_ptr<ScreenStream>>{
                    std::make_unique<FakeStream>(streamsAlive)};
            }};
    }

    [[nodiscard]] long closes() const { return std::ranges::count(portal.calls(), "Close:/session/fake"); }

    FakeScreenCastPortal portal;
    RegionsCallback onRegions;
    std::atomic<int> streamsAlive{0};
    std::uint32_t nodeId = 0;
    bool fdValid = false;
    bool failFactory = false;
    int factoryCalls = 0;
};

TEST_F(PortalScreenColorSourceTest, ConstructingTouchesNeitherThePortalNorPipeWire)
{
    const auto source = make();

    EXPECT_TRUE(portal.calls().empty());
    EXPECT_EQ(factoryCalls, 0);
    EXPECT_FALSE(source.running());
    EXPECT_TRUE(source.regions().empty());
}

TEST_F(PortalScreenColorSourceTest, StartNegotiatesThenOpensTheStreamOnTheGrantedNode)
{
    auto source = make();

    ASSERT_TRUE(source.start().has_value());

    EXPECT_TRUE(source.running());
    EXPECT_EQ(nodeId, 42U);
    EXPECT_TRUE(fdValid);
    EXPECT_EQ(streamsAlive, 1);
    EXPECT_TRUE(source.regions().empty()); // ainda sem quadro
}

TEST_F(PortalScreenColorSourceTest, RegionsFollowTheStreamCallback)
{
    auto source = make();
    ASSERT_TRUE(source.start().has_value());

    onRegions({RgbColor{1, 2, 3}, RgbColor{4, 5, 6}});

    EXPECT_EQ(source.regions(), (std::vector<RgbColor>{RgbColor{1, 2, 3}, RgbColor{4, 5, 6}}));
}

TEST_F(PortalScreenColorSourceTest, StopClosesStreamAndPortalSessionAndForgetsTheColors)
{
    auto source = make();
    ASSERT_TRUE(source.start().has_value());
    onRegions({RgbColor{9, 9, 9}});

    source.stop();

    EXPECT_FALSE(source.running());
    EXPECT_EQ(streamsAlive, 0);
    EXPECT_EQ(closes(), 1);
    EXPECT_TRUE(source.regions().empty());
}

TEST_F(PortalScreenColorSourceTest, StopIsIdempotentAndALateCallbackDoesNotResurrectColors)
{
    auto source = make();
    ASSERT_TRUE(source.start().has_value());
    source.stop();
    source.stop();

    onRegions({RgbColor{9, 9, 9}});

    EXPECT_EQ(closes(), 1);
    EXPECT_TRUE(source.regions().empty());
}

TEST_F(PortalScreenColorSourceTest, StartingTwiceNegotiatesOnce)
{
    auto source = make();
    ASSERT_TRUE(source.start().has_value());
    ASSERT_TRUE(source.start().has_value());

    EXPECT_EQ(factoryCalls, 1);
    EXPECT_EQ(std::ranges::count(portal.calls(), "CreateSession"), 1);
}

TEST_F(PortalScreenColorSourceTest, CanBeRestartedAfterStopAskingTheUserAgain)
{
    auto source = make();
    ASSERT_TRUE(source.start().has_value());
    source.stop();
    ASSERT_TRUE(source.start().has_value());

    EXPECT_EQ(std::ranges::count(portal.calls(), "CreateSession"), 2); // sem token: novo consentimento a cada sessão
    EXPECT_TRUE(source.running());
}

TEST_F(PortalScreenColorSourceTest, DeniedPermissionIsBackendUnavailableAndNoStreamIsOpened)
{
    portal.failAt("Start");
    auto source = make();

    const auto started = source.start();

    ASSERT_FALSE(started.has_value());
    EXPECT_EQ(started.error().code, ErrorCode::BackendUnavailable);
    EXPECT_FALSE(source.running());
    EXPECT_EQ(factoryCalls, 0);
    EXPECT_EQ(closes(), 1);
}

TEST_F(PortalScreenColorSourceTest, StreamFailureClosesThePortalSessionAndLeavesNothingRunning)
{
    failFactory = true;
    auto source = make();

    const auto started = source.start();

    ASSERT_FALSE(started.has_value());
    EXPECT_FALSE(source.running());
    EXPECT_EQ(closes(), 1);
    EXPECT_EQ(streamsAlive, 0);
}

TEST_F(PortalScreenColorSourceTest, DestroyingWhileRunningClosesEverything)
{
    {
        auto source = make();
        ASSERT_TRUE(source.start().has_value());
    }

    EXPECT_EQ(streamsAlive, 0);
    EXPECT_EQ(closes(), 1);
}

TEST_F(PortalScreenColorSourceTest, StopFromAnotherThreadCancelsAPendingPermissionRequest)
{
    portal.blockInStart();
    auto source = make();
    ghub::shared_kernel::Result<void> started;
    std::thread worker{[&] { started = source.start(); }};
    while (!portal.insideStart()) {
        std::this_thread::yield(); // espera por condição: o diálogo "está aberto"
    }

    source.stop();
    worker.join();

    ASSERT_FALSE(started.has_value());
    EXPECT_FALSE(source.running());
    EXPECT_EQ(factoryCalls, 0);
    EXPECT_EQ(closes(), 1);
}

} // namespace
