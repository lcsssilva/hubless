#include "fakes/FakeClock.hpp"

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST(FakeClock, DoesNotMove_WhenNotAdvanced)
{
    const ghub::fakes::FakeClock clock;
    EXPECT_EQ(clock.now(), clock.now());
}

TEST(FakeClock, MovesExactlyByAdvancedAmount)
{
    ghub::fakes::FakeClock clock;
    const auto before = clock.now();
    clock.advance(3s);
    EXPECT_EQ(clock.now() - before, 3s);
}
