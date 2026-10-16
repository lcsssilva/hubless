#include "modules/onboard_memory/infrastructure/FileOnboardBackupStore.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <sys/stat.h>

using ghub::onboard_memory::infrastructure::FileOnboardBackupStore;
using ghub::shared_kernel::ErrorCode;
namespace fs = std::filesystem;

namespace {

class FileOnboardBackupStoreTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() /
               ("ghub_onboard_backup_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
                ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::remove_all(root);
        dir = root / "ghub-linux" / "backup"; // não existe: o repositório cria
    }
    void TearDown() override { fs::remove_all(root); }

    static unsigned modeOf(const fs::path& path)
    {
        struct stat info{};
        EXPECT_EQ(::stat(path.c_str(), &info), 0);
        return info.st_mode & 07777U;
    }

    static std::vector<std::uint8_t> readFile(const fs::path& path)
    {
        std::ifstream in{path, std::ios::binary};
        return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    }

    static std::vector<std::uint8_t> bytes(std::size_t size, std::uint8_t seed)
    {
        std::vector<std::uint8_t> data(size);
        for (std::size_t i = 0; i < size; ++i) {
            data[i] = static_cast<std::uint8_t>(seed + i);
        }
        return data;
    }

    fs::path root;
    fs::path dir;
};

} // namespace

TEST_F(FileOnboardBackupStoreTest, LoadOfAnAbsentBackupIsEmptyNotAnError)
{
    FileOnboardBackupStore store{dir, "g703-4086-sector-"};

    const auto loaded = store.load(1);

    ASSERT_TRUE(loaded.has_value()) << loaded.error().detail;
    EXPECT_FALSE(loaded->has_value());
}

TEST_F(FileOnboardBackupStoreTest, SaveCreatesTheDirectoryPrivatelyAndTheFileWithTheExactBytes)
{
    FileOnboardBackupStore store{dir, "g703-4086-sector-"};
    const auto data = bytes(255, 7);

    const auto saved = store.saveIfAbsent(1, data);

    ASSERT_TRUE(saved.has_value()) << saved.error().detail;
    const auto file = dir / "g703-4086-sector-0001.bin";
    ASSERT_TRUE(fs::exists(file));
    EXPECT_EQ(readFile(file), data);
    EXPECT_EQ(modeOf(file), 0600U);
    EXPECT_EQ(modeOf(dir), 0700U);
    const auto loaded = store.load(1);
    ASSERT_TRUE(loaded.has_value() && loaded->has_value());
    EXPECT_EQ(**loaded, data);
}

TEST_F(FileOnboardBackupStoreTest, SaveNeverOverwritesAnExistingBackup)
{
    FileOnboardBackupStore store{dir, "g703-4086-sector-"};
    const auto original = bytes(255, 1);
    ASSERT_TRUE(store.saveIfAbsent(1, original).has_value());

    const auto again = store.saveIfAbsent(1, bytes(255, 99));

    ASSERT_TRUE(again.has_value()) << again.error().detail;
    EXPECT_EQ(readFile(dir / "g703-4086-sector-0001.bin"), original);
}

TEST_F(FileOnboardBackupStoreTest, SavingLeavesNoTemporaryFileBehind)
{
    FileOnboardBackupStore store{dir, "g703-4086-sector-"};
    ASSERT_TRUE(store.saveIfAbsent(1, bytes(255, 1)).has_value());
    ASSERT_TRUE(store.saveIfAbsent(1, bytes(255, 2)).has_value());

    std::size_t count = 0;
    for (const auto& entry : fs::directory_iterator{dir}) {
        EXPECT_EQ(entry.path().filename(), "g703-4086-sector-0001.bin");
        ++count;
    }
    EXPECT_EQ(count, 1U);
}

TEST_F(FileOnboardBackupStoreTest, EachSectorHasItsOwnFile)
{
    FileOnboardBackupStore store{dir, "g703-4086-sector-"};
    ASSERT_TRUE(store.saveIfAbsent(1, bytes(255, 1)).has_value());
    ASSERT_TRUE(store.saveIfAbsent(0x0101, bytes(255, 2)).has_value());

    EXPECT_TRUE(fs::exists(dir / "g703-4086-sector-0001.bin"));
    EXPECT_TRUE(fs::exists(dir / "g703-4086-sector-0101.bin"));
    EXPECT_EQ(**store.load(0x0101), bytes(255, 2));
    EXPECT_FALSE(store.load(2)->has_value());
}

TEST_F(FileOnboardBackupStoreTest, LoadRefusesAFileLargerThanTheSectorLimit)
{
    fs::create_directories(dir);
    std::ofstream{dir / "g703-4086-sector-0001.bin", std::ios::binary}.write(std::string(4097, 'x').c_str(), 4097);
    FileOnboardBackupStore store{dir, "g703-4086-sector-"};

    const auto loaded = store.load(1);

    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().code, ErrorCode::MalformedData);
}

TEST_F(FileOnboardBackupStoreTest, SaveRefusesEmptyOrOversizedData)
{
    FileOnboardBackupStore store{dir, "g703-4086-sector-"};

    EXPECT_EQ(store.saveIfAbsent(1, {}).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(store.saveIfAbsent(1, bytes(4097, 0)).error().code, ErrorCode::InvalidArgument);
    EXPECT_FALSE(fs::exists(dir / "g703-4086-sector-0001.bin"));
}

TEST_F(FileOnboardBackupStoreTest, UnwritableDirectoryIsBackendUnavailable)
{
    fs::create_directories(root);
    std::ofstream{root / "arquivo"} << "x";
    FileOnboardBackupStore store{root / "arquivo" / "backup", "g703-4086-sector-"}; // pai é um arquivo

    const auto saved = store.saveIfAbsent(1, bytes(255, 1));

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::BackendUnavailable);
}
