#include "modules/device_inventory/infrastructure/SysfsHidDiscovery.hpp"
#include "TempDir.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace ghub::device_inventory {
namespace {

namespace fs = std::filesystem;

using testing::TempDir;

TEST(SysfsHidDiscovery, ListsParsableHidNames)
{
    TempDir tree;
    // No sysfs real as entradas são links simbólicos; um deles pendente prova que o alvo não é seguido.
    fs::create_directory(tree.path() / "0003:046D:407C.000A");
    fs::create_directory_symlink("../../../devices/inexistente", tree.path() / "0003:046D:C539.0001");
    fs::create_directory(tree.path() / "lixo");

    SysfsHidDiscovery discovery(tree.path());
    const auto result = discovery.listDevices();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (std::vector<UsbId>{{0x046d, 0x407c}, {0x046d, 0xc539}}));
}

TEST(SysfsHidDiscovery, FailsUnavailable_WhenRootMissing)
{
    TempDir tree;
    SysfsHidDiscovery discovery(tree.path() / "ausente");

    const auto result = discovery.listDevices();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, shared_kernel::ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("ausente"), std::string::npos);
}

TEST(SysfsHidDiscovery, FailsUnavailable_WhenRootNotReadable)
{
    if (::geteuid() == 0) {
        GTEST_SKIP() << "root ignora permissões de diretório";
    }
    TempDir tree;
    const auto root = tree.path() / "hid";
    fs::create_directory(root);
    fs::create_directory(root / "0003:046D:407C.000A");
    fs::permissions(root, fs::perms::none);

    SysfsHidDiscovery discovery(root);
    const auto result = discovery.listDevices();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, shared_kernel::ErrorCode::BackendUnavailable);
}

} // namespace
} // namespace ghub::device_inventory
