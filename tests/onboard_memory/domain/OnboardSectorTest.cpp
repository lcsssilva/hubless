#include "modules/onboard_memory/domain/OnboardSector.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

namespace ghub::onboard_memory {
namespace {

using shared_kernel::ErrorCode;

std::vector<std::uint8_t> bytesOf(std::string_view text)
{
    return {text.begin(), text.end()};
}

TEST(OnboardSectorTest, RejectsSizeBelowMinimum)
{
    const auto sector = OnboardSector::fromBytes(std::vector<std::uint8_t>(15, 0));
    ASSERT_FALSE(sector.has_value());
    EXPECT_EQ(sector.error().code, ErrorCode::InvalidArgument);
}

TEST(OnboardSectorTest, RejectsSizeAboveMaximum)
{
    const auto sector = OnboardSector::fromBytes(std::vector<std::uint8_t>(4097, 0));
    ASSERT_FALSE(sector.has_value());
    EXPECT_EQ(sector.error().code, ErrorCode::InvalidArgument);
}

TEST(OnboardSectorTest, AcceptsBoundarySizesAndKeepsBytes)
{
    const auto small = OnboardSector::fromBytes(std::vector<std::uint8_t>(16, 7));
    const auto large = OnboardSector::fromBytes(std::vector<std::uint8_t>(4096, 7));
    ASSERT_TRUE(small.has_value());
    ASSERT_TRUE(large.has_value());
    EXPECT_EQ(small->size(), 16U);
    EXPECT_EQ(large->size(), 4096U);
    EXPECT_EQ(small->bytes(), std::vector<std::uint8_t>(16, 7));
}

TEST(OnboardSectorTest, ComputesCcittCheckValue)
{
    // Vetor de verificação padrão de CRC-16/CCITT-FALSE (semente 0xFFFF, poly 0x1021).
    EXPECT_EQ(OnboardSector::crc16Ccitt(bytesOf("123456789")), 0x29B1);
}

TEST(OnboardSectorTest, ReadsStoredCrcBigEndianFromLastTwoBytes)
{
    const auto sector = OnboardSector::fromBytes({fixtures::kSector0001.begin(), fixtures::kSector0001.end()});
    ASSERT_TRUE(sector.has_value());
    EXPECT_EQ(sector->storedCrc(), fixtures::kSector0001Crc);
}

TEST(OnboardSectorTest, ComputedCrcMatchesStoredOnRealSectors)
{
    const auto one = OnboardSector::fromBytes({fixtures::kSector0001.begin(), fixtures::kSector0001.end()});
    const auto two = OnboardSector::fromBytes({fixtures::kSector0002.begin(), fixtures::kSector0002.end()});
    ASSERT_TRUE(one.has_value() && two.has_value());
    EXPECT_EQ(one->computedCrc(), 0xded9);
    EXPECT_EQ(two->computedCrc(), 0x2cac);
    EXPECT_TRUE(one->hasValidCrc());
    EXPECT_TRUE(two->hasValidCrc());
}

TEST(OnboardSectorTest, DetectsCorruptedByte)
{
    std::vector<std::uint8_t> raw{fixtures::kSector0001.begin(), fixtures::kSector0001.end()};
    raw[5] ^= 0x01;
    const auto sector = OnboardSector::fromBytes(std::move(raw));
    ASSERT_TRUE(sector.has_value());
    EXPECT_FALSE(sector->hasValidCrc());
}

TEST(OnboardSectorTest, WithCrcRewritesOnlyLastTwoBytes)
{
    std::vector<std::uint8_t> raw{fixtures::kSector0001.begin(), fixtures::kSector0001.end()};
    raw[5] ^= 0x01;
    const auto broken = OnboardSector::fromBytes(raw);
    ASSERT_TRUE(broken.has_value());
    const auto fixed = broken->withCrc();
    EXPECT_TRUE(fixed.hasValidCrc());
    EXPECT_EQ(fixed.size(), broken->size());
    for (std::size_t i = 0; i + 2 < raw.size(); ++i) {
        EXPECT_EQ(fixed.bytes()[i], raw[i]) << "byte " << i;
    }
    EXPECT_FALSE(broken->hasValidCrc()); // original imutável
}

TEST(OnboardSectorTest, EqualityComparesBytes)
{
    const auto a = OnboardSector::fromBytes(std::vector<std::uint8_t>(16, 1));
    const auto b = OnboardSector::fromBytes(std::vector<std::uint8_t>(16, 1));
    const auto c = OnboardSector::fromBytes(std::vector<std::uint8_t>(16, 2));
    ASSERT_TRUE(a && b && c);
    EXPECT_EQ(*a, *b);
    EXPECT_NE(*a, *c);
}

} // namespace
} // namespace ghub::onboard_memory
