#include "modules/lighting/infrastructure/G915HidppControl.hpp"

#include "fakes/FakeClock.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

using ghub::lighting::infrastructure::G915HidppControl;
using ghub::platform::hidpp::HidppNodeLocator;
using ghub::shared_kernel::ErrorCode;

class G915HidppControlTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        root = std::filesystem::temp_directory_path() /
               ("ghub_g915_control_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
                ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "sys" / "class" / "hidraw");
        std::filesystem::create_directories(root / "dev");
    }
    void TearDown() override { std::filesystem::remove_all(root); }

    void addNode(const std::string& name, const std::string& hidId, const std::string& phys)
    {
        const auto dir = root / "sys" / "class" / "hidraw" / name / "device";
        std::filesystem::create_directories(dir);
        std::ofstream{dir / "uevent"} << "HID_ID=" << hidId << "\nHID_PHYS=" << phys << "\n";
    }

    std::filesystem::path root;
    ghub::fakes::FakeClock clock;
};

} // namespace

TEST_F(G915HidppControlTest, AbsentKeyboardIsUnavailable)
{
    G915HidppControl control{HidppNodeLocator{root / "sys", root / "dev"}, clock};

    const auto session = control.open();

    ASSERT_FALSE(session.has_value());
    EXPECT_EQ(session.error().code, ErrorCode::BackendUnavailable);
}

TEST_F(G915HidppControlTest, OpeningTheReceiverNodeWritesNothing)
{
    addNode("hidraw7", "0003:0000046D:0000C547", "usb-0000:11:00.4-2.4/input2");
    addNode("hidraw9", "0003:0000046D:0000407C", "usb-0000:11:00.4-2.4/input2:1");
    const auto node = root / "dev" / "hidraw7";
    std::ofstream{node}.close(); // arquivo comum no lugar do nó: registra qualquer escrita
    G915HidppControl control{HidppNodeLocator{root / "sys", root / "dev"}, clock};

    const auto session = control.open();

    // Um arquivo comum aceita O_RDWR; o ponto é que abrir não escreve nada.
    if (session.has_value()) {
        EXPECT_EQ(std::filesystem::file_size(node), 0U);
    }
}
