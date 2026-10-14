#include "modules/lighting/infrastructure/G915HidppLighting.hpp"

#include "fakes/FakeClock.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using ghub::lighting::infrastructure::G915HidppLighting;
using ghub::lighting::infrastructure::HidppNodeLocator;
using ghub::shared_kernel::ErrorCode;

class G915HidppLightingTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        root = std::filesystem::temp_directory_path() /
               ("ghub_g915_lighting_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
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

TEST_F(G915HidppLightingTest, AbsentKeyboardIsUnavailable)
{
    G915HidppLighting lighting{HidppNodeLocator{root / "sys", root / "dev"}, clock};

    const auto session = lighting.open();

    ASSERT_FALSE(session.has_value());
    EXPECT_EQ(session.error().code, ErrorCode::BackendUnavailable);
}

TEST_F(G915HidppLightingTest, OpeningTheReceiverNodeSendsNothing)
{
    addNode("hidraw7", "0003:0000046D:0000C547", "usb-0000:11:00.4-2.4/input2");
    addNode("hidraw9", "0003:0000046D:0000407C", "usb-0000:11:00.4-2.4/input2:1");
    std::ofstream{root / "dev" / "hidraw7"}.close(); // arquivo comum no lugar do nó: registra qualquer escrita
    G915HidppLighting lighting{HidppNodeLocator{root / "sys", root / "dev"}, clock};

    const auto session = lighting.open();

    ASSERT_TRUE(session.has_value()) << session.error().detail;
    EXPECT_NE(*session, nullptr);
    EXPECT_EQ(std::filesystem::file_size(root / "dev" / "hidraw7"), 0U);
}

TEST_F(G915HidppLightingTest, TraceSinkReachesTheChannelCreatedByOpen)
{
    addNode("hidraw7", "0003:0000046D:0000C547", "usb-0000:11:00.4-2.4/input2");
    addNode("hidraw9", "0003:0000046D:0000407C", "usb-0000:11:00.4-2.4/input2:1");
    std::ofstream{root / "dev" / "hidraw7"}.close(); // arquivo comum: a leitura da resposta falha ou esgota o prazo
    std::vector<ghub::platform::hidpp::HidppCommandTrace> traces;
    G915HidppLighting lighting{HidppNodeLocator{root / "sys", root / "dev"}, clock,
                               [&traces](const auto& trace) { traces.push_back(trace); }};
    auto session = lighting.open();
    ASSERT_TRUE(session.has_value()) << session.error().detail;

    (void)(*session)->beginLive();

    EXPECT_EQ(traces.size(), 1U) << "o primeiro comando da sessão gera exatamente um traço";
}

} // namespace
