#include "modules/audio_processing/infrastructure/PipeWireFilterChainArgs.hpp"

#include <gtest/gtest.h>

#include <string>

namespace ghub::audio_processing {
namespace {

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

constexpr const char* kSource = "alsa_input.usb-Logitech_G733_Gaming_Headset-00.mono-fallback";

TEST(NoiseSuppressionArgsTest, GeneratesExactGraphAndStreamProperties)
{
    const auto args = buildNoiseSuppressionArgs(VoiceThreshold::create(50).value(), kSource);
    ASSERT_TRUE(args.has_value());
    EXPECT_TRUE(contains(*args, "node.description = \"G733 + Redução de ruído\""));
    EXPECT_TRUE(contains(*args, "audio.rate = 48000"));
    EXPECT_TRUE(contains(*args, "audio.channels = 1"));
    EXPECT_TRUE(contains(*args, "audio.position = [ MONO ]"));
    EXPECT_TRUE(contains(*args, "filter.graph = { nodes = [ { type = ladspa name = rnnoise plugin = "
                                "\"librnnoise_ladspa\" label = noise_suppressor_mono control = { \"VAD Threshold "
                                "(%)\" 50.000000 } } ] }"));
    EXPECT_TRUE(
        contains(*args, std::string{"capture.props = { node.name = \"ghub.g733.mic.input\" target.object = \""} +
                            kSource + "\" node.dont-fallback = true node.dont-move = true node.passive = true }"));
    EXPECT_TRUE(contains(*args, "playback.props = { node.name = \"ghub.g733.mic\" media.class = Audio/Source }"));
    EXPECT_FALSE(contains(*args, "remote.name"));
    EXPECT_FALSE(contains(*args, "inputs ="));
}

TEST(NoiseSuppressionArgsTest, ThresholdAppearsInTheControl)
{
    const auto args = buildNoiseSuppressionArgs(VoiceThreshold::create(99).value(), kSource);
    ASSERT_TRUE(args.has_value());
    EXPECT_TRUE(contains(*args, "\"VAD Threshold (%)\" 99.000000"));
}

TEST(NoiseSuppressionArgsTest, RemoteNameIsAppendedWhenGiven)
{
    const auto args = buildNoiseSuppressionArgs(VoiceThreshold::create(50).value(), kSource, "/tmp/ghub-pw-x/sock");
    ASSERT_TRUE(args.has_value());
    EXPECT_TRUE(contains(*args, "remote.name = \"/tmp/ghub-pw-x/sock\""));
}

TEST(NoiseSuppressionArgsTest, UnsafeNamesAreInvalidArgument)
{
    for (const char* bad : {"", "a\"b", "a\\b", "a\nb"}) {
        const auto source = buildNoiseSuppressionArgs(VoiceThreshold::create(50).value(), bad);
        ASSERT_FALSE(source.has_value()) << bad;
        EXPECT_EQ(source.error().code, shared_kernel::ErrorCode::InvalidArgument) << bad;
    }
    const auto remote = buildNoiseSuppressionArgs(VoiceThreshold::create(50).value(), kSource, "a\"b");
    ASSERT_FALSE(remote.has_value());
    EXPECT_EQ(remote.error().code, shared_kernel::ErrorCode::InvalidArgument);
}

TEST(NoiseSuppressionArgsTest, ControlUpdateIsTheNamedPort)
{
    const auto updates = buildNoiseSuppressionControlUpdates(VoiceThreshold::create(73).value());
    ASSERT_EQ(updates.size(), 1U);
    EXPECT_EQ(updates[0].name, "rnnoise:VAD Threshold (%)");
    EXPECT_FLOAT_EQ(updates[0].value, 73.0F);
}

} // namespace
} // namespace ghub::audio_processing
