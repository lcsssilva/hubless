#include "modules/device_inventory/domain/DeviceIdentity.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace ghub::device_inventory {
namespace {

constexpr std::array<std::uint8_t, 3> kPrefix{'M', 'P', 'K'};

FirmwareVersion firmware(std::uint8_t number)
{
    return FirmwareVersion{0, kPrefix, number, 0x00, 0x0001, false};
}

TEST(DeviceIdentity, DefaultsToEverythingUnknown)
{
    const DeviceIdentity identity;

    EXPECT_FALSE(identity.name().has_value());
    EXPECT_FALSE(identity.deviceType().has_value());
    EXPECT_TRUE(identity.firmwares().empty());
    EXPECT_TRUE(identity.featureIds().empty());
    EXPECT_FALSE(identity.firmwareTruncated());
    EXPECT_FALSE(identity.featuresTruncated());
}

TEST(DeviceIdentity, KeepsWhatWasInformed)
{
    const DeviceIdentity identity{DeviceIdentityParts{
        .name = "G915", .deviceType = 0, .firmwares = {firmware(9)}, .featureIds = {0x0001, 0x1001}}};

    EXPECT_EQ(identity.name(), "G915");
    EXPECT_EQ(identity.deviceType(), 0);
    ASSERT_EQ(identity.firmwares().size(), 1U);
    EXPECT_EQ(identity.firmwares()[0].versionText(), "MPK 09.00 b0001");
    EXPECT_EQ(identity.featureIds(), (std::vector<std::uint16_t>{0x0001, 0x1001}));
}

TEST(DeviceIdentity, CutsFirmwareEntriesAboveTheLimitAndSaysSo)
{
    DeviceIdentityParts parts;
    for (std::uint8_t i = 0; i < DeviceIdentity::maxFirmwareEntries + 1; ++i) {
        parts.firmwares.push_back(firmware(i));
    }

    const DeviceIdentity identity{std::move(parts)};

    EXPECT_EQ(identity.firmwares().size(), DeviceIdentity::maxFirmwareEntries);
    EXPECT_TRUE(identity.firmwareTruncated());
    EXPECT_FALSE(identity.featuresTruncated());
}

TEST(DeviceIdentity, CutsFeaturesAboveTheLimitAndSaysSo)
{
    DeviceIdentityParts parts;
    for (std::uint16_t i = 0; i < DeviceIdentity::maxFeatures + 5; ++i) {
        parts.featureIds.push_back(i);
    }

    const DeviceIdentity identity{std::move(parts)};

    EXPECT_EQ(identity.featureIds().size(), DeviceIdentity::maxFeatures);
    EXPECT_EQ(identity.featureIds().back(), DeviceIdentity::maxFeatures - 1);
    EXPECT_TRUE(identity.featuresTruncated());
}

TEST(DeviceIdentity, KeepsTheTruncationFlagsReportedByTheReader)
{
    DeviceIdentityParts parts;
    parts.firmwareTruncated = true;
    parts.featuresTruncated = true;

    const DeviceIdentity identity{std::move(parts)};

    EXPECT_TRUE(identity.firmwareTruncated());
    EXPECT_TRUE(identity.featuresTruncated());
}

TEST(DeviceName, BuildsTheNameFromTheInformedBytes)
{
    const std::vector<std::uint8_t> bytes{'G', '9', '1', '5'};

    EXPECT_EQ(deviceNameFromBytes(bytes), "G915");
}

TEST(DeviceName, ReplacesNonPrintableBytesWithQuestionMark)
{
    const std::vector<std::uint8_t> bytes{'A', 0x00, 'B', 0x7F, 0xC3, 0x1F, ' '};

    EXPECT_EQ(deviceNameFromBytes(bytes), "A?B??? ");
}

TEST(DeviceName, EmptyOrTooLongIsNotInformed)
{
    EXPECT_FALSE(deviceNameFromBytes({}).has_value());
    EXPECT_TRUE(deviceNameFromBytes(std::vector<std::uint8_t>(DeviceIdentity::maxNameLength, 'x')).has_value());
    EXPECT_FALSE(deviceNameFromBytes(std::vector<std::uint8_t>(DeviceIdentity::maxNameLength + 1, 'x')).has_value());
}

} // namespace
} // namespace ghub::device_inventory
