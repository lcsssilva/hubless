#include "LeaseTestSupport.hpp"

#include "fakes/FakeClock.hpp"
#include "modules/device_access/domain/AccessRegistry.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <latch>
#include <optional>
#include <thread>
#include <vector>

using ghub::device_access::AccessRegistry;
using ghub::device_access::DeviceLease;
using ghub::device_access::test::request;
using ghub::shared_kernel::ErrorCode;

TEST(AccessRegistryConcurrency, GrantsAtMostOneLease_UnderContention)
{
    constexpr std::size_t threadCount = 16;
    constexpr int rounds = 50;
    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};

    for (int round = 0; round < rounds; ++round) {
        std::vector<std::optional<DeviceLease>> granted(threadCount);
        std::vector<std::optional<ErrorCode>> rejected(threadCount);
        std::latch start{static_cast<std::ptrdiff_t>(threadCount)};
        {
            std::vector<std::jthread> workers;
            workers.reserve(threadCount);
            for (std::size_t i = 0; i < threadCount; ++i) {
                workers.emplace_back([&, i] {
                    start.arrive_and_wait();
                    auto lease = registry.acquire(request("hid:shared"));
                    if (lease) {
                        granted[i].emplace(std::move(*lease));
                    } else {
                        rejected[i] = lease.error().code;
                    }
                });
            }
        }

        const auto grantedCount = std::ranges::count_if(granted, [](const auto& l) { return l.has_value(); });
        const auto busyCount = std::ranges::count(rejected, std::optional{ErrorCode::DeviceBusy});
        ASSERT_EQ(grantedCount, 1) << "rodada " << round;
        ASSERT_EQ(busyCount, static_cast<std::ptrdiff_t>(threadCount) - 1) << "rodada " << round;
    }
}

TEST(AccessRegistryConcurrency, ReleaseAndAcquireFromManyThreads_KeepsExclusivity)
{
    constexpr std::size_t threadCount = 8;
    constexpr int iterations = 2000;
    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};
    std::atomic<int> holders{0};
    std::atomic<int> maxObserved{0};

    {
        std::vector<std::jthread> workers;
        workers.reserve(threadCount);
        for (std::size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back([&] {
                for (int n = 0; n < iterations; ++n) {
                    auto lease = registry.acquire(request("hid:shared"));
                    if (!lease) {
                        continue;
                    }
                    const int now = holders.fetch_add(1) + 1;
                    int seen = maxObserved.load();
                    while (now > seen && !maxObserved.compare_exchange_weak(seen, now)) {
                    }
                    holders.fetch_sub(1);
                }
            });
        }
    }

    EXPECT_EQ(maxObserved.load(), 1);
}
