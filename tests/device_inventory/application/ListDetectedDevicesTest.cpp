#include "modules/device_inventory/application/ListDetectedDevices.hpp"
#include "fakes/FakeDeviceDiscovery.hpp"

#include <gtest/gtest.h>

namespace ghub::device_inventory {
namespace {

using Paths = std::vector<ConnectionPath>;

constexpr UsbId g915Receiver{.vendorId = 0x046d, .productId = 0x407c};
constexpr UsbId g915Cable{.vendorId = 0x046d, .productId = 0xc33e};
constexpr UsbId g703Receiver{.vendorId = 0x046d, .productId = 0x4086};
constexpr UsbId g703Cable{.vendorId = 0x046d, .productId = 0xc090};
constexpr UsbId brio100{.vendorId = 0x046d, .productId = 0x094c};

TEST(ListDetectedDevices, ReportsEachModel_InFixedOrder)
{
    fakes::FakeDeviceDiscovery discovery;
    discovery.setResult(std::vector<UsbId>{g703Receiver, g915Receiver});

    const auto detections = ListDetectedDevices({&discovery}).execute();

    ASSERT_EQ(detections.size(), 4U);
    EXPECT_EQ(detections[0].model, DeviceModel::G915);
    EXPECT_EQ(detections[1].model, DeviceModel::G703);
    EXPECT_EQ(detections[2].model, DeviceModel::G733);
    EXPECT_EQ(detections[3].model, DeviceModel::Brio100);
    ASSERT_TRUE(detections[0].paths.has_value());
    EXPECT_EQ(*detections[0].paths, Paths{ConnectionPath::LightspeedReceiver});
    ASSERT_TRUE(detections[1].paths.has_value());
    EXPECT_EQ(*detections[1].paths, Paths{ConnectionPath::LightspeedReceiver});
    ASSERT_TRUE(detections[2].paths.has_value());
    EXPECT_TRUE(detections[2].paths->empty());
}

TEST(ListDetectedDevices, MergesPaths_WithoutDuplicates)
{
    fakes::FakeDeviceDiscovery discovery;
    discovery.setResult(std::vector<UsbId>{g703Cable, g703Receiver, g703Receiver});

    const auto detections = ListDetectedDevices({&discovery}).execute();

    ASSERT_TRUE(detections[1].paths.has_value());
    EXPECT_EQ(*detections[1].paths, (Paths{ConnectionPath::LightspeedReceiver, ConnectionPath::UsbCable}));
}

TEST(ListDetectedDevices, ReportsG915_OnReceiverAndCable)
{
    fakes::FakeDeviceDiscovery discovery;
    // O c33e expõe três interfaces HID; cada uma vira uma entrada em /sys/bus/hid/devices.
    discovery.setResult(std::vector<UsbId>{g915Cable, g915Cable, g915Cable, g915Receiver});

    const auto detections = ListDetectedDevices({&discovery}).execute();

    ASSERT_TRUE(detections[0].paths.has_value());
    EXPECT_EQ(*detections[0].paths, (Paths{ConnectionPath::LightspeedReceiver, ConnectionPath::UsbCable}));
    ASSERT_TRUE(detections[1].paths.has_value());
    EXPECT_TRUE(detections[1].paths->empty());
}

TEST(ListDetectedDevices, MarksAllUnavailable_WhenDiscoveryFails)
{
    fakes::FakeDeviceDiscovery discovery;
    discovery.setResult(shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "sem sysfs"));

    const auto detections = ListDetectedDevices({&discovery}).execute();

    ASSERT_EQ(detections.size(), 4U);
    for (const auto& detection : detections) {
        ASSERT_FALSE(detection.paths.has_value());
        EXPECT_EQ(detection.paths.error().code, shared_kernel::ErrorCode::BackendUnavailable);
        EXPECT_EQ(detection.paths.error().detail, "sem sysfs");
    }
}

TEST(ListDetectedDevices, IgnoresUnknownIds)
{
    fakes::FakeDeviceDiscovery discovery;
    discovery.setResult(
        std::vector<UsbId>{{.vendorId = 0x046d, .productId = 0xc539}, {.vendorId = 0x1234, .productId = 0x407c}});

    const auto detections = ListDetectedDevices({&discovery}).execute();

    ASSERT_EQ(detections.size(), 4U);
    for (const auto& detection : detections) {
        ASSERT_TRUE(detection.paths.has_value());
        EXPECT_TRUE(detection.paths->empty());
    }
}

TEST(ListDetectedDevices, CombinesDiscoveries)
{
    fakes::FakeDeviceDiscovery hid;
    fakes::FakeDeviceDiscovery video;
    hid.setResult(std::vector<UsbId>{g915Receiver});
    video.setResult(std::vector<UsbId>{brio100});

    const auto detections = ListDetectedDevices({{&hid, &video}}).execute();

    ASSERT_EQ(detections.size(), 4U);
    ASSERT_TRUE(detections[0].paths.has_value());
    EXPECT_EQ(*detections[0].paths, Paths{ConnectionPath::LightspeedReceiver});
    ASSERT_TRUE(detections[3].paths.has_value());
    EXPECT_EQ(*detections[3].paths, Paths{ConnectionPath::UsbCable});
}

TEST(ListDetectedDevices, KeepsDetectedModels_WhenOtherDiscoveryFails)
{
    fakes::FakeDeviceDiscovery hid;
    fakes::FakeDeviceDiscovery video;
    hid.setResult(std::vector<UsbId>{g915Receiver});
    video.setResult(shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "sem video4linux"));

    const auto detections = ListDetectedDevices({{&hid, &video}}).execute();

    ASSERT_TRUE(detections[0].paths.has_value());
    EXPECT_EQ(*detections[0].paths, Paths{ConnectionPath::LightspeedReceiver});
}

TEST(ListDetectedDevices, MarksUnavailable_OnlyModelsWithoutPath_WhenADiscoveryFails)
{
    fakes::FakeDeviceDiscovery hid;
    fakes::FakeDeviceDiscovery video;
    hid.setResult(std::vector<UsbId>{g915Receiver});
    video.setResult(shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "sem video4linux"));

    const auto detections = ListDetectedDevices({{&hid, &video}}).execute();

    EXPECT_TRUE(detections[0].paths.has_value());
    for (const std::size_t i : {1U, 2U, 3U}) {
        ASSERT_FALSE(detections[i].paths.has_value());
        EXPECT_EQ(detections[i].paths.error().detail, "sem video4linux");
    }
}

TEST(ListDetectedDevices, UsesFirstFailure_AsError)
{
    fakes::FakeDeviceDiscovery first;
    fakes::FakeDeviceDiscovery second;
    first.setResult(shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "primeira"));
    second.setResult(shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "segunda"));

    const auto detections = ListDetectedDevices({{&first, &second}}).execute();

    for (const auto& detection : detections) {
        ASSERT_FALSE(detection.paths.has_value());
        EXPECT_EQ(detection.paths.error().detail, "primeira");
    }
}

} // namespace
} // namespace ghub::device_inventory
