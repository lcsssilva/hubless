#include "modules/audio_processing/infrastructure/PipeWireGraphQueries.hpp"

#include <gtest/gtest.h>

namespace ghub::audio_processing {
namespace {

constexpr const char* kRealSource = "alsa_input.usb-Logitech_G733_Gaming_Headset_0000000000000000-00.mono-fallback";

GraphObject device(std::uint32_t id, const char* vendor, const char* product)
{
    return {id, {{"media.class", "Audio/Device"}, {"device.vendor.id", vendor}, {"device.product.id", product}}};
}

GraphObject node(std::uint32_t id, const char* name, const char* deviceId, const char* mediaClass)
{
    GraphObject object{id, {{"node.name", name}, {"media.class", mediaClass}}};
    if (deviceId != nullptr) {
        object.props["device.id"] = deviceId;
    }
    return object;
}

TEST(NoiseSuppressionQueriesTest, FindsRealMachineG733SourceAndIgnoresSinksAndOtherDevices)
{
    GraphSnapshot snapshot;
    snapshot.devices = {device(46, "0x1002", "0xab38"), device(47, "0x046d", "0x0afe")};
    snapshot.nodes = {node(40, "alsa_input.pci-0000_01_00.1.analog", "46", "Audio/Source"),
                      node(54, "alsa_output.usb-Logitech_G733.analog-stereo", "47", "Audio/Sink"),
                      node(53, kRealSource, "47", "Audio/Source")};
    const auto found = findHeadsetSourceName(snapshot);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, kRealSource);
}

TEST(NoiseSuppressionQueriesTest, WithoutG733SourceIsBackendUnavailable)
{
    GraphSnapshot snapshot;
    snapshot.devices = {device(48, "0x046d", "0xc08b"), device(47, "0x046d", "0x0afe")};
    snapshot.nodes = {node(41, "alsa_input.other", "48", "Audio/Source"), node(42, "virtual", nullptr, "Audio/Source"),
                      node(54, "alsa_output.g733", "47", "Audio/Sink")};
    const auto found = findHeadsetSourceName(snapshot);
    ASSERT_FALSE(found.has_value());
    EXPECT_EQ(found.error().code, shared_kernel::ErrorCode::BackendUnavailable);
    EXPECT_EQ(found.error().detail, "G733 não encontrado");
}

TEST(NoiseSuppressionQueriesTest, DefaultNodeValueRoundTripsAndMalformedNamesTheKey)
{
    EXPECT_EQ(makeDefaultNodeValue("ghub.g733.mic"), "{\"name\":\"ghub.g733.mic\"}");
    EXPECT_EQ(parseDefaultNodeValue(makeDefaultNodeValue("a\"b"), "default.configured.audio.source").value(), "a\"b");
    const auto bad = parseDefaultNodeValue("{}", "default.configured.audio.source");
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().code, shared_kernel::ErrorCode::MalformedData);
    EXPECT_EQ(bad.error().detail, "valor de default.configured.audio.source mal formado");
}

} // namespace
} // namespace ghub::audio_processing
