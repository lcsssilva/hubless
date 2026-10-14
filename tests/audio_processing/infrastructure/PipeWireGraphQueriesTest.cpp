#include "modules/audio_processing/infrastructure/PipeWireGraphQueries.hpp"

#include <gtest/gtest.h>

namespace ghub::audio_processing {
namespace {

constexpr const char* kRealSink = "alsa_output.usb-Logitech_G733_Gaming_Headset_0000000000000000-00.analog-stereo";

GraphObject device(std::uint32_t id, const char* vendor, const char* product)
{
    return {id, {{"media.class", "Audio/Device"}, {"device.vendor.id", vendor}, {"device.product.id", product}}};
}

GraphObject sink(std::uint32_t id, const char* name, const char* deviceId, const char* mediaClass = "Audio/Sink")
{
    GraphObject node{id, {{"node.name", name}, {"media.class", mediaClass}}};
    if (deviceId != nullptr) {
        node.props["device.id"] = deviceId;
    }
    return node;
}

TEST(PipeWireGraphQueriesTest, FindsRealMachineG733SinkAndIgnoresHdmi)
{
    GraphSnapshot snapshot;
    snapshot.devices = {device(46, "0x1002", "0xab38"), device(47, "0x046d", "0x0afe")};
    snapshot.nodes = {sink(40, "alsa_output.pci-0000_01_00.1.hdmi-stereo", "46"),
                      sink(53, "alsa_input.usb-Logitech_G733.mono-fallback", "47", "Audio/Source"),
                      sink(54, kRealSink, "47")};
    const auto found = findHeadsetSinkName(snapshot);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, kRealSink);
}

TEST(PipeWireGraphQueriesTest, AcceptsUppercaseHexVendorAndProduct)
{
    GraphSnapshot snapshot;
    snapshot.devices = {device(47, "0x046D", "0x0AFE")};
    snapshot.nodes = {sink(54, kRealSink, "47")};
    EXPECT_TRUE(findHeadsetSinkName(snapshot).has_value());
}

TEST(PipeWireGraphQueriesTest, WithoutG733IsBackendUnavailable)
{
    GraphSnapshot snapshot;
    snapshot.devices = {device(46, "0x1002", "0xab38"), device(48, "0x046d", "0xc08b")}; // outro produto Logitech
    snapshot.nodes = {sink(40, "alsa_output.hdmi", "46"), sink(41, "alsa_output.other", "48"),
                      sink(42, "virtual", nullptr)};
    const auto found = findHeadsetSinkName(snapshot);
    ASSERT_FALSE(found.has_value());
    EXPECT_EQ(found.error().code, shared_kernel::ErrorCode::BackendUnavailable);
    EXPECT_EQ(found.error().detail, "G733 não encontrado");
}

TEST(PipeWireGraphQueriesTest, G733DeviceWithoutSinkNodeIsNotFound)
{
    GraphSnapshot snapshot;
    snapshot.devices = {device(47, "0x046d", "0x0afe")};
    snapshot.nodes = {sink(53, "alsa_input.mono", "47", "Audio/Source")};
    EXPECT_FALSE(findHeadsetSinkName(snapshot).has_value());
}

TEST(PipeWireGraphQueriesTest, DefaultSinkValueRoundTrips)
{
    EXPECT_EQ(makeDefaultSinkValue("ghub.g733.eq"), "{\"name\":\"ghub.g733.eq\"}");
    const auto parsed = parseDefaultSinkValue("{\"name\":\"alsa_output.hdmi-stereo\"}");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, "alsa_output.hdmi-stereo");
    EXPECT_EQ(parseDefaultSinkValue(makeDefaultSinkValue("a\"b\\c")).value(), "a\"b\\c");
}

TEST(PipeWireGraphQueriesTest, MalformedDefaultSinkValueIsMalformedData)
{
    for (const char* bad : {"", "not json", "{}", "{\"name\":3}", "{\"name\":\"x\"", "[\"x\"]", "{\"other\":\"x\"}"}) {
        const auto parsed = parseDefaultSinkValue(bad);
        ASSERT_FALSE(parsed.has_value()) << bad;
        EXPECT_EQ(parsed.error().code, shared_kernel::ErrorCode::MalformedData) << bad;
    }
}

} // namespace
} // namespace ghub::audio_processing
