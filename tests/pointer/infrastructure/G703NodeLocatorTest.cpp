#include "modules/pointer/infrastructure/G703NodeLocator.hpp"
#include "fakes/FakeClock.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using ghub::pointer::infrastructure::G703Node;
using ghub::pointer::infrastructure::G703NodeLocator;
using ghub::pointer::infrastructure::G703OnboardConnector;
using ghub::shared_kernel::ErrorCode;

namespace {

class G703NodeLocatorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        root = std::filesystem::temp_directory_path() /
               ("ghub_pointer_sysfs_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
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

    [[nodiscard]] G703NodeLocator locator() const { return G703NodeLocator{root, "/dev"}; }

    std::filesystem::path root;
};

} // namespace

TEST_F(G703NodeLocatorTest, FindsTheReceiverInterfaceTwoAndTheDeviceIndex)
{
    addNode("hidraw5", "0003:0000046D:0000C539", "usb-0000:11:00.4-2.2/input0");
    addNode("hidraw6", "0003:0000046D:0000C539", "usb-0000:11:00.4-2.2/input1");
    addNode("hidraw7", "0003:0000046D:0000C539", "usb-0000:11:00.4-2.2/input2");
    addNode("hidraw9", "0003:0000046D:00004086", "usb-0000:11:00.4-2.2/input2:2");

    const auto node = locator().locate();

    ASSERT_TRUE(node.has_value()) << node.error().detail;
    EXPECT_EQ(*node, (G703Node{.devicePath = "/dev/hidraw7", .deviceIndex = 2}));
}

TEST_F(G703NodeLocatorTest, ReceiverMustShareTheUsbPortWithTheMouse)
{
    addNode("hidraw2", "0003:0000046D:0000C539", "usb-0000:11:00.4-1/input2");
    addNode("hidraw7", "0003:0000046D:0000C539", "usb-0000:11:00.4-2.2/input2");
    addNode("hidraw9", "0003:0000046D:00004086", "usb-0000:11:00.4-2.2/input2:5");

    const auto node = locator().locate();

    ASSERT_TRUE(node.has_value());
    EXPECT_EQ(node->devicePath, "/dev/hidraw7");
    EXPECT_EQ(node->deviceIndex, 5);
}

TEST_F(G703NodeLocatorTest, IndexOutsideOneToSixIsIgnored)
{
    addNode("hidraw7", "0003:0000046D:0000C539", "usb-x-2/input2");
    addNode("hidraw8", "0003:0000046D:00004086", "usb-x-2/input2:0");
    addNode("hidraw9", "0003:0000046D:00004086", "usb-x-2/input2:7");
    addNode("hidraw10", "0003:0000046D:00004086", "usb-x-2/input2:abc");

    EXPECT_EQ(locator().locate().error().code, ErrorCode::BackendUnavailable);
}

TEST_F(G703NodeLocatorTest, DoesNotAcceptAnotherReceiverOrAnotherDevice)
{
    addNode("hidraw7", "0003:0000046D:0000C547", "usb-x-2/input2");
    addNode("hidraw8", "0003:0000046D:00004086", "usb-x-2/input2:2");
    addNode("hidraw9", "0003:0000046D:0000407C", "usb-x-2/input2:1");

    EXPECT_EQ(locator().locate().error().code, ErrorCode::BackendUnavailable);
}

TEST_F(G703NodeLocatorTest, MouseWithoutItsReceiverNodeIsNotFound)
{
    addNode("hidraw9", "0003:0000046D:00004086", "usb-x-2/input2:1");
    addNode("hidraw5", "0003:0000046D:0000C539", "usb-x-2/input0");

    EXPECT_FALSE(locator().locate().has_value());
}

TEST_F(G703NodeLocatorTest, MalformedUeventEntriesAreSkippedAndTheDevRootIsInjected)
{
    addNode("hidraw1", "lixo", "usb-x-2/input2");
    addNode("hidraw7", "0003:0000046D:0000C539", "usb-x-2/input2");
    addNode("hidraw9", "0003:0000046D:00004086", "usb-x-2/input2:3");
    const G703NodeLocator custom{root, "/tmp/fakedev"};

    const auto node = custom.locate();

    ASSERT_TRUE(node.has_value());
    EXPECT_EQ(node->devicePath, "/tmp/fakedev/hidraw7");
}

TEST_F(G703NodeLocatorTest, MissingSysfsTreeIsBackendUnavailable)
{
    const G703NodeLocator missing{root / "nao_existe", "/dev"};

    EXPECT_EQ(missing.locate().error().code, ErrorCode::BackendUnavailable);
}

TEST_F(G703NodeLocatorTest, ConnectorReportsNotFoundWithoutOpeningAnything)
{
    ghub::fakes::FakeClock clock;
    G703OnboardConnector connector{locator(), clock};

    const auto memory = connector.open();

    ASSERT_FALSE(memory.has_value());
    EXPECT_EQ(memory.error().code, ErrorCode::BackendUnavailable);
}
