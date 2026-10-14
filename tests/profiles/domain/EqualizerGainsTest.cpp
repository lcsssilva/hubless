#include "modules/profiles/domain/EqualizerGains.hpp"

#include <gtest/gtest.h>

using ghub::profiles::EqualizerGains;
using ghub::shared_kernel::ErrorCode;

TEST(EqualizerGains, AcceptsExactlyTenBands)
{
    const auto gains = EqualizerGains::create({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    ASSERT_TRUE(gains.has_value());
    EXPECT_EQ(gains->values()[0], 1);
    EXPECT_EQ(gains->values()[9], 10);
}

TEST(EqualizerGains, RejectsNineBands)
{
    const auto gains = EqualizerGains::create(std::vector<int>(9, 0));
    ASSERT_FALSE(gains.has_value());
    EXPECT_EQ(gains.error().code, ErrorCode::InvalidArgument);
}

TEST(EqualizerGains, RejectsElevenBands)
{
    const auto gains = EqualizerGains::create(std::vector<int>(11, 0));
    ASSERT_FALSE(gains.has_value());
    EXPECT_EQ(gains.error().code, ErrorCode::InvalidArgument);
}

TEST(EqualizerGains, DoesNotValidateGainRange)
{
    // A faixa -12..12 é do módulo audio_processing (ADR-0020).
    const auto gains = EqualizerGains::create({13, -99, 0, 0, 0, 0, 0, 0, 0, 1000});
    ASSERT_TRUE(gains.has_value());
    EXPECT_EQ(gains->values()[0], 13);
}
