#include "modules/onboard_memory/domain/OnboardDirectory.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace ghub::onboard_memory {
namespace {

using shared_kernel::ErrorCode;

std::vector<std::uint8_t> rawDirectory()
{
    return {fixtures::kDirectory0000.begin(), fixtures::kDirectory0000.end()};
}

TEST(OnboardDirectoryTest, ParsesRealDirectory)
{
    const auto directory = OnboardDirectory::parse(*OnboardSector::fromBytes(rawDirectory()));
    ASSERT_TRUE(directory.has_value());
    EXPECT_EQ(directory->entries().size(), 5U);
    EXPECT_TRUE(directory->isEnabled(0x0001));
    for (std::uint16_t sector = 2; sector <= 5; ++sector) {
        EXPECT_FALSE(directory->isEnabled(sector)) << sector;
    }
}

TEST(OnboardDirectoryTest, AbsentSectorIsNotEnabled)
{
    const auto directory = OnboardDirectory::parse(*OnboardSector::fromBytes(rawDirectory()));
    ASSERT_TRUE(directory.has_value());
    EXPECT_FALSE(directory->isEnabled(0x0006));
    EXPECT_FALSE(directory->isEnabled(0x0000));
}

TEST(OnboardDirectoryTest, RejectsInvalidCrc)
{
    auto raw = rawDirectory();
    raw[2] = 0x00;
    const auto directory = OnboardDirectory::parse(*OnboardSector::fromBytes(std::move(raw)));
    ASSERT_FALSE(directory.has_value());
    EXPECT_EQ(directory.error().code, ErrorCode::MalformedData);
}

TEST(OnboardDirectoryTest, StopsAtEndMarkerEvenIfLaterBytesLookLikeEntries)
{
    auto raw = rawDirectory();
    // Depois do marcador 0xFFFF (bytes 20..21), uma "entrada" falsa não pode valer.
    raw[24] = 0x00;
    raw[25] = 0x07;
    raw[26] = 0x01;
    const auto directory = OnboardDirectory::parse(OnboardSector::fromBytes(std::move(raw))->withCrc());
    ASSERT_TRUE(directory.has_value());
    EXPECT_FALSE(directory->isEnabled(0x0007));
}

TEST(OnboardDirectoryTest, ReadsUntilTheEndWithoutMarker)
{
    std::vector<std::uint8_t> raw(16, 0x00);
    raw[0] = 0x00;
    raw[1] = 0x01;
    raw[2] = 0x01;
    raw[4] = 0x00;
    raw[5] = 0x02;
    raw[6] = 0x01;
    const auto directory = OnboardDirectory::parse(OnboardSector::fromBytes(std::move(raw))->withCrc());
    ASSERT_TRUE(directory.has_value());
    EXPECT_TRUE(directory->isEnabled(0x0001));
    EXPECT_TRUE(directory->isEnabled(0x0002));
    EXPECT_EQ(directory->entries().size(), 3U);
}

} // namespace
} // namespace ghub::onboard_memory
