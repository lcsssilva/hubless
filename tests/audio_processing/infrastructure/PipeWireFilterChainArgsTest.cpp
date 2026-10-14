#include "modules/audio_processing/infrastructure/PipeWireFilterChainArgs.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

namespace ghub::audio_processing {
namespace {

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

std::size_t count(const std::string& text, const std::string& part)
{
    std::size_t n = 0;
    for (auto pos = text.find(part); pos != std::string::npos; pos = text.find(part, pos + part.size())) {
        ++n;
    }
    return n;
}

constexpr const char* kSink = "alsa_output.usb-Logitech_G733_Gaming_Headset-00.analog-stereo";

TEST(PipeWireFilterChainArgsTest, GeneratesExactSinkAndStreamProperties)
{
    const auto args = buildFilterChainArgs(EqualizerCurve::flat(), kSink);
    ASSERT_TRUE(args.has_value());
    EXPECT_TRUE(contains(*args, "node.description = \"G733 + EQ (G HUB Linux)\""));
    EXPECT_TRUE(contains(*args, "audio.channels = 2"));
    EXPECT_TRUE(contains(*args, "audio.position = [ FL FR ]"));
    EXPECT_TRUE(contains(*args, "capture.props = { node.name = \"ghub.g733.eq\" media.class = Audio/Sink }"));
    EXPECT_TRUE(
        contains(*args, std::string{"playback.props = { node.name = \"ghub.g733.eq.output\" target.object = \""} +
                            kSink + "\" node.dont-fallback = true node.dont-move = true node.passive = true }"));
}

TEST(PipeWireFilterChainArgsTest, GraphHasPreampMixerAndTenPeakingBandsWithFixedQ)
{
    const auto args = buildFilterChainArgs(EqualizerCurve::flat(), kSink);
    ASSERT_TRUE(args.has_value());
    EXPECT_EQ(count(*args, "label = bq_peaking"), 10U);
    EXPECT_EQ(count(*args, "\"Q\" = 1.410000"), 10U);
    EXPECT_EQ(count(*args, "label = mixer"), 1U);
    EXPECT_TRUE(contains(*args, "name = pre type = builtin label = mixer control = { \"Gain 1\" = 1.000000 }"));
    EXPECT_TRUE(contains(*args, "name = eq_1 type = builtin label = bq_peaking control = { \"Freq\" = 32.000000 \"Q\" "
                                "= 1.410000 \"Gain\" = 0.000000 }"));
    EXPECT_TRUE(contains(*args, "name = eq_10 type = builtin label = bq_peaking control = { \"Freq\" = 16000.000000 "
                                "\"Q\" = 1.410000 \"Gain\" = 0.000000 }"));
}

TEST(PipeWireFilterChainArgsTest, ChainsPreampThroughAllBandsAndDeclaresEndpoints)
{
    const auto args = buildFilterChainArgs(EqualizerCurve::flat(), kSink);
    ASSERT_TRUE(args.has_value());
    EXPECT_TRUE(contains(*args, "{ output = \"pre:Out\" input = \"eq_1:In\" }"));
    EXPECT_TRUE(contains(*args, "{ output = \"eq_1:Out\" input = \"eq_2:In\" }"));
    EXPECT_TRUE(contains(*args, "{ output = \"eq_9:Out\" input = \"eq_10:In\" }"));
    EXPECT_EQ(count(*args, "output ="), 10U);
    EXPECT_TRUE(contains(*args, "inputs = [ \"pre:In 1\" ]"));
    EXPECT_TRUE(contains(*args, "outputs = [ \"eq_10:Out\" ]"));
}

TEST(PipeWireFilterChainArgsTest, InitialControlsFollowCurveAndPreampIsLinear)
{
    const auto curve = EqualizerCurve::flat().withBand(0, 12).value().withBand(9, -6).value();
    const auto args = buildFilterChainArgs(curve, kSink);
    ASSERT_TRUE(args.has_value());
    EXPECT_TRUE(contains(*args, "\"Freq\" = 32.000000 \"Q\" = 1.410000 \"Gain\" = 12.000000"));
    EXPECT_TRUE(contains(*args, "\"Freq\" = 16000.000000 \"Q\" = 1.410000 \"Gain\" = -6.000000"));
    EXPECT_TRUE(contains(*args, "\"Gain 1\" = 0.251189")); // 10^(-12/20)
}

TEST(PipeWireFilterChainArgsTest, RejectsSinkNamesThatCouldBreakOutOfTheString)
{
    for (const std::string& bad : {std::string{"a\"b"}, std::string{"a\\b"}, std::string{"a\nb"}, std::string{"a\tb"},
                                   std::string{"a\x01"
                                               "b"},
                                   std::string{}}) {
        const auto args = buildFilterChainArgs(EqualizerCurve::flat(), bad);
        ASSERT_FALSE(args.has_value()) << bad;
        EXPECT_EQ(args.error().code, shared_kernel::ErrorCode::InvalidArgument);
    }
}

TEST(PipeWireFilterChainArgsTest, RemoteNameIsAddedOnlyWhenGivenAndValidated)
{
    const auto without = buildFilterChainArgs(EqualizerCurve::flat(), kSink);
    ASSERT_TRUE(without.has_value());
    EXPECT_FALSE(contains(*without, "remote.name"));

    const auto with = buildFilterChainArgs(EqualizerCurve::flat(), kSink, "/tmp/ghub-pw-x/sock");
    ASSERT_TRUE(with.has_value());
    EXPECT_TRUE(contains(*with, " remote.name = \"/tmp/ghub-pw-x/sock\" "));

    const auto bad = buildFilterChainArgs(EqualizerCurve::flat(), kSink, "/tmp/a\"b");
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().code, shared_kernel::ErrorCode::InvalidArgument);
}

TEST(PipeWireFilterChainArgsTest, ControlUpdatesCoverPreampAndEveryBand)
{
    const auto curve = EqualizerCurve::flat().withBand(2, 7).value().withBand(4, -3).value();
    const auto updates = buildControlUpdates(curve);
    ASSERT_EQ(updates.size(), 11U);
    EXPECT_EQ(updates[0].name, "pre:Gain 1");
    EXPECT_NEAR(updates[0].value, 0.446684F, 1e-5F); // 10^(-7/20)
    EXPECT_EQ(updates[1].name, "eq_1:Gain");
    EXPECT_FLOAT_EQ(updates[1].value, 0.0F);
    EXPECT_EQ(updates[3].name, "eq_3:Gain");
    EXPECT_FLOAT_EQ(updates[3].value, 7.0F);
    EXPECT_EQ(updates[5].name, "eq_5:Gain");
    EXPECT_FLOAT_EQ(updates[5].value, -3.0F);
    EXPECT_EQ(updates[10].name, "eq_10:Gain");
}

} // namespace
} // namespace ghub::audio_processing
