#include "platform/hidpp/HidrawDevice.hpp"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

using ghub::platform::hidpp::HidIoFailure;
using ghub::platform::hidpp::HidrawDevice;

namespace {

// socketpair(SOCK_SEQPACKET) preserva limites de mensagem como o hidraw preserva limites de relatório.
class HidrawDeviceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::array<int, 2> fds{};
        ASSERT_EQ(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds.data()), 0);
        device.emplace(fds[0]);
        peer = fds[1];
    }
    void TearDown() override
    {
        if (peer >= 0) {
            ::close(peer);
        }
    }

    std::optional<ghub::platform::hidpp::HidrawDevice> device;
    int peer = -1;
};

} // namespace

TEST_F(HidrawDeviceTest, WriteDeliversTheWholeReport)
{
    const std::vector<std::uint8_t> report{0x11, 0x01, 0x07, 0x5B, 0xAA};

    const auto written = device->write(report);

    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(*written, report.size());
    std::array<std::uint8_t, 32> received{};
    const auto size = ::recv(peer, received.data(), received.size(), 0);
    ASSERT_EQ(size, static_cast<ssize_t>(report.size()));
    EXPECT_TRUE(std::equal(report.begin(), report.end(), received.begin()));
}

TEST_F(HidrawDeviceTest, ReadReturnsOneReportAtATime)
{
    const std::array<std::uint8_t, 3> first{1, 2, 3};
    const std::array<std::uint8_t, 2> second{4, 5};
    ASSERT_EQ(::send(peer, first.data(), first.size(), 0), 3);
    ASSERT_EQ(::send(peer, second.data(), second.size(), 0), 2);
    std::array<std::uint8_t, 64> buffer{};

    const auto one = device->read(buffer, std::chrono::milliseconds{100});
    ASSERT_TRUE(one.has_value());
    EXPECT_EQ(*one, 3U);
    EXPECT_EQ(buffer[0], 1);
    const auto two = device->read(buffer, std::chrono::milliseconds{100});
    ASSERT_TRUE(two.has_value());
    EXPECT_EQ(*two, 2U);
    EXPECT_EQ(buffer[0], 4);
}

TEST_F(HidrawDeviceTest, ReadWithoutDataFailsWithTimedOutInsteadOfBlocking)
{
    std::array<std::uint8_t, 64> buffer{};

    const auto result = device->read(buffer, std::chrono::milliseconds{1});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().failure, ghub::platform::hidpp::HidIoFailure::TimedOut);
}

TEST_F(HidrawDeviceTest, ReadWithZeroTimeoutPollsOnce)
{
    std::array<std::uint8_t, 64> buffer{};

    const auto result = device->read(buffer, std::chrono::milliseconds{0});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().failure, ghub::platform::hidpp::HidIoFailure::TimedOut);
}

TEST_F(HidrawDeviceTest, ReadAfterPeerClosedIsAnIoErrorNotATimeout)
{
    ::close(peer);
    peer = -1;
    std::array<std::uint8_t, 64> buffer{};

    const auto result = device->read(buffer, std::chrono::milliseconds{100});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().failure, ghub::platform::hidpp::HidIoFailure::Io);
}

TEST_F(HidrawDeviceTest, WriteAfterPeerClosedIsAnIoError)
{
    ::close(peer);
    peer = -1;
    const std::array<std::uint8_t, 2> report{1, 2};
    // Soquete (não hidraw) gera SIGPIPE ao escrever no par fechado; o teste só quer o erro.
    const auto previous = std::signal(SIGPIPE, SIG_IGN);

    const auto result = device->write(report);

    std::signal(SIGPIPE, previous);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().failure, ghub::platform::hidpp::HidIoFailure::Io);
    EXPECT_NE(result.error().systemError, 0);
}

TEST(HidrawDeviceLifecycle, DestructorClosesTheDescriptor)
{
    std::array<int, 2> fds{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds.data()), 0);
    {
        ghub::platform::hidpp::HidrawDevice device{fds[0]};
    }
    errno = 0;
    EXPECT_EQ(::fcntl(fds[0], F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);
    ::close(fds[1]);
}

TEST(HidrawDeviceOpen, FailsForMissingPath)
{
    const auto result = ghub::platform::hidpp::HidrawDevice::open("/nonexistent/hidraw99");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ghub::shared_kernel::ErrorCode::BackendUnavailable);
}

TEST(HidrawDeviceOpen, OpensReadWriteWithCloexec)
{
    const auto path = std::filesystem::temp_directory_path() / "ghub_lighting_open_test.bin";
    {
        std::ofstream{path} << "x";
    }

    {
        const auto result = ghub::platform::hidpp::HidrawDevice::open(path);
        ASSERT_TRUE(result.has_value());
        bool found = false;
        for (const auto& entry : std::filesystem::directory_iterator("/proc/self/fd")) {
            std::error_code error;
            if (std::filesystem::read_symlink(entry.path(), error) != path) {
                continue;
            }
            found = true;
            std::ifstream info{"/proc/self/fdinfo/" + entry.path().filename().string()};
            std::string line;
            std::string flags;
            while (std::getline(info, line)) {
                if (line.starts_with("flags:")) {
                    flags = line.substr(6);
                }
            }
            const auto value = std::stoul(flags, nullptr, 8);
            EXPECT_NE(value & O_CLOEXEC, 0U);
            EXPECT_EQ(value & O_ACCMODE, static_cast<unsigned long>(O_RDWR));
        }
        EXPECT_TRUE(found);
    }
    std::filesystem::remove(path);
}
