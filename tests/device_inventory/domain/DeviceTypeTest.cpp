#include "modules/device_inventory/domain/DeviceType.hpp"

#include <gtest/gtest.h>

namespace ghub::device_inventory {
namespace {

TEST(DeviceType, NamesTheTypesOfTheHidpp20Specification)
{
    EXPECT_EQ(deviceTypeText(0), "teclado");
    EXPECT_EQ(deviceTypeText(1), "controle remoto");
    EXPECT_EQ(deviceTypeText(2), "teclado numérico");
    EXPECT_EQ(deviceTypeText(3), "mouse");
    EXPECT_EQ(deviceTypeText(4), "touchpad");
    EXPECT_EQ(deviceTypeText(5), "trackball");
    EXPECT_EQ(deviceTypeText(6), "apresentador");
    EXPECT_EQ(deviceTypeText(7), "receptor");
}

TEST(DeviceType, NumbersTheTypesWithoutAName)
{
    EXPECT_EQ(deviceTypeText(8), "outro (8)"); // o G733 informa 8 (captura C-00)
    EXPECT_EQ(deviceTypeText(200), "outro (200)");
}

} // namespace
} // namespace ghub::device_inventory
