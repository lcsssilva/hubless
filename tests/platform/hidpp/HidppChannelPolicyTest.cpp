// ADR-0017 decisão 2: o HidppChannel só envia o que a política injetada permitir.
#include "fakes/FakeClock.hpp"
#include "fakes/FakeHidReportIo.hpp"
#include "platform/hidpp/HidppChannel.hpp"
#include "platform/hidpp/HidppCommandPolicy.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace hidpp = ghub::platform::hidpp;

namespace {

class DenyAllPolicy final : public hidpp::HidppCommandPolicy {
public:
    [[nodiscard]] bool isAllowed(std::uint16_t, std::uint8_t, std::span<const std::uint8_t>) const noexcept override
    {
        return false;
    }
};

// Recusa quando o primeiro byte dos parâmetros é zero; permite todo o resto.
class RejectZeroFirstParamPolicy final : public hidpp::HidppCommandPolicy {
public:
    [[nodiscard]] bool isAllowed(std::uint16_t, std::uint8_t,
                                 std::span<const std::uint8_t> params) const noexcept override
    {
        return params.empty() || params[0] != 0;
    }
};

struct Rig {
    explicit Rig(const hidpp::HidppCommandPolicy& policy)
    {
        auto fake = std::make_unique<ghub::fakes::FakeHidReportIo>(&clock);
        io = fake.get();
        io->setResponder([](const std::vector<std::uint8_t>& frame) {
            std::vector<std::uint8_t> reply(frame);
            return std::vector<std::vector<std::uint8_t>>{reply};
        });
        channel.emplace(std::move(fake), clock, policy);
    }
    ghub::fakes::FakeClock clock;
    ghub::fakes::FakeHidReportIo* io = nullptr;
    std::optional<hidpp::HidppChannel> channel;
};

} // namespace

TEST(HidppChannelPolicy, DenyAllPolicySendsNoByteAndReportsPolicyDenied)
{
    const DenyAllPolicy policy;
    Rig rig{policy};

    const auto direct = rig.channel->call(0x01, 0x8071, 0);
    const auto root = rig.channel->call(0x01, 0x0000, 0);

    ASSERT_FALSE(direct.has_value());
    EXPECT_EQ(direct.error().kind, hidpp::HidppErrorKind::PolicyDenied);
    ASSERT_FALSE(root.has_value());
    EXPECT_EQ(root.error().kind, hidpp::HidppErrorKind::PolicyDenied);
    EXPECT_TRUE(rig.io->written().empty());
}

TEST(HidppChannelPolicy, DecisionUsesTheParamsBeingSent)
{
    const RejectZeroFirstParamPolicy policy;
    Rig rig{policy};
    const std::vector<std::uint8_t> zero{0x00};
    const std::vector<std::uint8_t> one{0x01};

    const auto denied = rig.channel->call(0x01, 0x0000, 0, zero);
    EXPECT_FALSE(denied.has_value());
    EXPECT_TRUE(rig.io->written().empty());

    const auto allowed = rig.channel->call(0x01, 0x0000, 0, one);
    EXPECT_TRUE(allowed.has_value());
    EXPECT_EQ(rig.io->written().size(), 1U);
}

TEST(HidppChannelPolicy, InternalRootGetFeatureQueryIsAlsoSubjectToThePolicy)
{
    const RejectZeroFirstParamPolicy policy; // a consulta Root leva o ID da feature (0x00,0x71) -> params[0]==0x00
    Rig rig{policy};

    const auto result = rig.channel->call(0x01, 0x0071, 1);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, hidpp::HidppErrorKind::PolicyDenied);
    EXPECT_TRUE(rig.io->written().empty());
}
