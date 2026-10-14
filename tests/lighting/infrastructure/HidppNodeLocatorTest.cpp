#include "modules/lighting/infrastructure/HidppNodeLocator.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using ghub::lighting::infrastructure::HidppNode;
using ghub::lighting::infrastructure::HidppNodeLocator;
using ghub::shared_kernel::ErrorCode;

namespace {

class HidppNodeLocatorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        root = std::filesystem::temp_directory_path() /
               ("ghub_lighting_sysfs_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
                ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "class" / "hidraw");
    }
    void TearDown() override { std::filesystem::remove_all(root); }

    void addNode(const std::string& name, const std::string& hidId, const std::string& phys)
    {
        const auto dir = root / "class" / "hidraw" / name / "device";
        std::filesystem::create_directories(dir);
        std::ofstream{dir / "uevent"} << "DRIVER=hid-generic\nHID_ID=" << hidId << "\nHID_NAME=Teste\nHID_PHYS=" << phys
                                      << "\nHID_UNIQ=\nMODALIAS=hid:b0003\n";
    }

    [[nodiscard]] HidppNodeLocator locator() const { return HidppNodeLocator{root, "/dev"}; }

    std::filesystem::path root;
};

} // namespace

TEST_F(HidppNodeLocatorTest, FindsG915ThroughItsReceiverAndTheDeviceIndex)
{
    addNode("hidraw5", "0003:0000046D:0000C547", "usb-0000:11:00.4-2.4/input0");
    addNode("hidraw6", "0003:0000046D:0000C547", "usb-0000:11:00.4-2.4/input1");
    addNode("hidraw7", "0003:0000046D:0000C547", "usb-0000:11:00.4-2.4/input2");
    addNode("hidraw9", "0003:0000046D:0000407C", "usb-0000:11:00.4-2.4/input2:1");

    const auto node = locator().locateG915();

    ASSERT_TRUE(node.has_value()) << node.error().detail;
    EXPECT_EQ(node->devicePath, "/dev/hidraw7");
    EXPECT_EQ(node->deviceIndex, 1);
}

TEST_F(HidppNodeLocatorTest, G915ReceiverMustShareTheUsbPortWithTheKeyboard)
{
    addNode("hidraw2", "0003:0000046D:0000C547", "usb-0000:11:00.4-1/input2");
    addNode("hidraw7", "0003:0000046D:0000C547", "usb-0000:11:00.4-2.4/input2");
    addNode("hidraw9", "0003:0000046D:0000407C", "usb-0000:11:00.4-2.4/input2:3");

    const auto node = locator().locateG915();

    ASSERT_TRUE(node.has_value());
    EXPECT_EQ(node->devicePath, "/dev/hidraw7");
    EXPECT_EQ(node->deviceIndex, 3);
}

TEST_F(HidppNodeLocatorTest, G915IgnoresOtherDevicesOnTheSameReceiverPort)
{
    addNode("hidraw7", "0003:0000046D:0000C547", "usb-0000:11:00.4-2.4/input2");
    addNode("hidraw8", "0003:0000046D:00004086", "usb-0000:11:00.4-2.4/input2:2");

    const auto node = locator().locateG915();

    ASSERT_FALSE(node.has_value());
    EXPECT_EQ(node.error().code, ErrorCode::BackendUnavailable);
}

TEST_F(HidppNodeLocatorTest, G915WithoutReceiverNodeIsNotFound)
{
    addNode("hidraw9", "0003:0000046D:0000407C", "usb-0000:11:00.4-2.4/input2:1");
    addNode("hidraw5", "0003:0000046D:0000C547", "usb-0000:11:00.4-2.4/input0");

    EXPECT_FALSE(locator().locateG915().has_value());
}

TEST_F(HidppNodeLocatorTest, FindsG733InterfaceThreeWithBroadcastIndex)
{
    addNode("hidraw1", "0003:0000046D:00000AFE", "usb-0000:0e:00.0-2/input1");
    addNode("hidraw3", "0003:0000046D:00000AFE", "usb-0000:0e:00.0-2/input3");

    const auto node = locator().locateG733();

    ASSERT_TRUE(node.has_value());
    EXPECT_EQ(node->devicePath, "/dev/hidraw3");
    EXPECT_EQ(node->deviceIndex, 0xFF);
}

TEST_F(HidppNodeLocatorTest, MissingDevicesAreBackendUnavailable)
{
    addNode("hidraw4", "0003:0000048D:00005702", "usb-0000:0e:00.0-6/input0");

    EXPECT_EQ(locator().locateG733().error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(locator().locateG915().error().code, ErrorCode::BackendUnavailable);
}

TEST_F(HidppNodeLocatorTest, MissingSysfsTreeIsBackendUnavailable)
{
    const HidppNodeLocator missing{root / "nao_existe", "/dev"};

    EXPECT_EQ(missing.locateG733().error().code, ErrorCode::BackendUnavailable);
}

TEST_F(HidppNodeLocatorTest, MalformedUeventEntriesAreSkipped)
{
    addNode("hidraw1", "lixo", "usb-x/input3");
    addNode("hidraw3", "0003:0000046D:00000AFE", "usb-0000:0e:00.0-2/input3");

    const auto node = locator().locateG733();

    ASSERT_TRUE(node.has_value());
    EXPECT_EQ(node->devicePath, "/dev/hidraw3");
}

TEST_F(HidppNodeLocatorTest, UsesTheInjectedDevRoot)
{
    addNode("hidraw3", "0003:0000046D:00000AFE", "usb-0000:0e:00.0-2/input3");
    const HidppNodeLocator custom{root, "/tmp/fakedev"};

    EXPECT_EQ(custom.locateG733()->devicePath, "/tmp/fakedev/hidraw3");
}
