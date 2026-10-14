#include "modules/device_inventory/domain/DeviceCatalog.hpp"

#include <gtest/gtest.h>

namespace ghub::device_inventory {
namespace {

TEST(DeviceCatalog, Identifies_ObservedIds)
{
    EXPECT_EQ(identifyDevice({0x046d, 0x407c}), (CatalogEntry{DeviceModel::G915, ConnectionPath::LightspeedReceiver}));
    EXPECT_EQ(identifyDevice({0x046d, 0x4086}), (CatalogEntry{DeviceModel::G703, ConnectionPath::LightspeedReceiver}));
    EXPECT_EQ(identifyDevice({0x046d, 0xc090}), (CatalogEntry{DeviceModel::G703, ConnectionPath::UsbCable}));
    EXPECT_EQ(identifyDevice({0x046d, 0x0afe}), (CatalogEntry{DeviceModel::G733, ConnectionPath::UsbDongle}));
}

// PID com fio observado em evidence/2026-10-13-g915-cabo.md.
TEST(DeviceCatalog, Identifies_G915_OnUsbCable)
{
    EXPECT_EQ(identifyDevice({0x046d, 0xc33e}), (CatalogEntry{DeviceModel::G915, ConnectionPath::UsbCable}));
}

TEST(DeviceCatalog, Identifies_Brio100)
{
    EXPECT_EQ(identifyDevice({0x046d, 0x094c}), (CatalogEntry{DeviceModel::Brio100, ConnectionPath::UsbCable}));
}

TEST(DeviceCatalog, ModelName_Brio100)
{
    EXPECT_EQ(modelName(DeviceModel::Brio100), "Brio 100");
}

TEST(DeviceCatalog, KnowsFourModels_InDisplayOrder)
{
    EXPECT_EQ(kDeviceModels.size(), 4U);
    EXPECT_EQ(kDeviceModels.back(), DeviceModel::Brio100);
}

TEST(UsbModalias, Parses_UsbInterfaceModalias)
{
    constexpr std::string_view real = "usb:v046Dp094Cd9915dcEFdsc02dp01ic0Eisc01ip00in00";
    const UsbId expected{.vendorId = 0x046d, .productId = 0x094c};
    EXPECT_EQ(parseUsbModalias(real), expected);
    EXPECT_EQ(parseUsbModalias("usb:v046Dp094Cd9915dcEFdsc02dp01ic0Eisc01ip00in00\n"), expected);
    EXPECT_EQ(parseUsbModalias("usb:v046dp094cd9915"), expected);
    EXPECT_EQ(parseUsbModalias("usb:v046Dp094C"), expected);
    EXPECT_EQ(parseUsbModalias("usb:v046Dp094C\n"), expected);
}

TEST(UsbModalias, Rejects_Malformed)
{
    EXPECT_FALSE(parseUsbModalias("").has_value());
    EXPECT_FALSE(parseUsbModalias("pci:v00008086d00001234").has_value());
    EXPECT_FALSE(parseUsbModalias("usb:v046Dp09").has_value());
    EXPECT_FALSE(parseUsbModalias("usb:v046Gp094C").has_value());
    EXPECT_FALSE(parseUsbModalias("usb:x046Dp094C").has_value());
    EXPECT_FALSE(parseUsbModalias("usb:v046Dx094C").has_value()); // marcador do PID errado
    EXPECT_FALSE(parseUsbModalias("hid:b0003g0001v0000046Dp0000407C").has_value());
}

TEST(DeviceCatalog, DoesNotIdentify_OtherIds)
{
    EXPECT_FALSE(identifyDevice({0x046d, 0xc539}).has_value());
    EXPECT_FALSE(identifyDevice({0x046d, 0xc547}).has_value());
    EXPECT_FALSE(identifyDevice({0x046d, 0x0aff}).has_value());
    EXPECT_FALSE(identifyDevice({0x1234, 0x407c}).has_value());
}

TEST(DeviceCatalog, ModelName_IsCommercialName)
{
    EXPECT_EQ(modelName(DeviceModel::G915), "G915");
    EXPECT_EQ(modelName(DeviceModel::G703), "G703");
    EXPECT_EQ(modelName(DeviceModel::G733), "G733");
}

} // namespace
} // namespace ghub::device_inventory
