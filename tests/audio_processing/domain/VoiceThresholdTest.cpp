#include "modules/audio_processing/domain/VoiceThreshold.hpp"

#include <gtest/gtest.h>

using ghub::audio_processing::VoiceThreshold;
using ghub::shared_kernel::ErrorCode;

TEST(VoiceThreshold, AcceptsBounds)
{
    EXPECT_EQ(VoiceThreshold::create(0)->percent(), 0);
    EXPECT_EQ(VoiceThreshold::create(99)->percent(), 99);
}

TEST(VoiceThreshold, RejectsBelowZero)
{
    const auto threshold = VoiceThreshold::create(-1);
    ASSERT_FALSE(threshold.has_value());
    EXPECT_EQ(threshold.error().code, ErrorCode::InvalidArgument);
}

TEST(VoiceThreshold, RejectsAbove99)
{
    const auto threshold = VoiceThreshold::create(100);
    ASSERT_FALSE(threshold.has_value());
    EXPECT_EQ(threshold.error().code, ErrorCode::InvalidArgument);
}

TEST(VoiceThreshold, DefaultIsFiftyPercent)
{
    EXPECT_EQ(VoiceThreshold::standard().percent(), 50);
}
