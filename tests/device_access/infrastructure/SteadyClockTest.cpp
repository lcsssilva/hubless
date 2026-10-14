#include "modules/device_access/infrastructure/SteadyClock.hpp"

#include <gtest/gtest.h>

#include <chrono>

using ghub::device_access::SteadyClock;

TEST(SteadyClock, NeverGoesBackwards)
{
    const SteadyClock clock;

    const auto first = clock.now();
    const auto second = clock.now();

    EXPECT_LE(first, second);
}

TEST(SteadyClock, IsBackedByTheSystemSteadyClock)
{
    const SteadyClock clock;

    const auto before = std::chrono::steady_clock::now();
    const auto reading = clock.now();
    const auto after = std::chrono::steady_clock::now();

    EXPECT_GE(reading, before);
    EXPECT_LE(reading, after);
}
