#include "modules/device_inventory/infrastructure/SysfsVideoDiscovery.hpp"
#include "TempDir.hpp"

#include <gtest/gtest.h>

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace ghub::device_inventory {
namespace {

namespace fs = std::filesystem;
using testing::TempDir;

constexpr std::string_view brioModalias = "usb:v046Dp094Cd9915dcEFdsc02dp01ic0Eisc01ip00in00\n";

void writeModalias(const fs::path& root, const std::string& entry, std::string_view content)
{
    fs::create_directories(root / entry / "device");
    std::ofstream(root / entry / "device" / "modalias", std::ios::binary) << content;
}

TEST(SysfsVideoDiscovery, ListsUsbIdsFromModalias_WithoutDuplicates)
{
    TempDir tree;
    writeModalias(tree.path(), "video0", brioModalias);
    writeModalias(tree.path(), "video1", brioModalias);
    writeModalias(tree.path(), "video2", "usb:v1234p0001d0100\n");

    SysfsVideoDiscovery discovery(tree.path());
    const auto result = discovery.listDevices();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (std::vector<UsbId>{{0x046d, 0x094c}, {0x1234, 0x0001}}));
}

TEST(SysfsVideoDiscovery, IgnoresEntriesWithoutModalias_OrNonUsb)
{
    TempDir tree;
    fs::create_directory(tree.path() / "video0"); // sem device
    writeModalias(tree.path(), "video1", "platform:vivid\n");
    fs::create_directory_symlink("../../devices/inexistente", tree.path() / "video2");
    fs::create_directories(tree.path() / "video3" / "device"); // device sem modalias
    writeModalias(tree.path(), "video4", brioModalias);

    SysfsVideoDiscovery discovery(tree.path());
    const auto result = discovery.listDevices();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (std::vector<UsbId>{{0x046d, 0x094c}}));
}

TEST(SysfsVideoDiscovery, FailsUnavailable_WhenRootMissing)
{
    TempDir tree;
    SysfsVideoDiscovery discovery(tree.path() / "ausente");

    const auto result = discovery.listDevices();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, shared_kernel::ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("ausente"), std::string::npos);
}

TEST(SysfsVideoDiscovery, FailsUnavailable_WhenRootNotReadable)
{
    if (::geteuid() == 0) {
        GTEST_SKIP() << "root ignora permissões de diretório";
    }
    TempDir tree;
    const auto root = tree.path() / "v4l";
    writeModalias(root, "video0", brioModalias);
    fs::permissions(root, fs::perms::none);

    SysfsVideoDiscovery discovery(root);
    const auto result = discovery.listDevices();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, shared_kernel::ErrorCode::BackendUnavailable);
}

TEST(SysfsVideoDiscovery, ReadsAtMost256Bytes)
{
    TempDir tree;
    // Prefixo válido seguido de lixo muito além do limite: o resultado é definido e a leitura termina.
    writeModalias(tree.path(), "video0", "usb:v046Dp094Cd" + std::string(1U << 20U, 'x'));
    // Modalias cujo prefixo só aparece depois do limite não pode ser reconhecido.
    writeModalias(tree.path(), "video1", std::string(300, ' ') + "usb:v1234p0001");

    SysfsVideoDiscovery discovery(tree.path());
    const auto result = discovery.listDevices();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (std::vector<UsbId>{{0x046d, 0x094c}}));
}

} // namespace
} // namespace ghub::device_inventory
