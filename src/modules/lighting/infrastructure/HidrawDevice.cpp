#include "modules/lighting/infrastructure/HidrawDevice.hpp"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <limits>
#include <system_error>

namespace ghub::lighting::infrastructure {

namespace {

HidIoError ioError(int systemError)
{
    return HidIoError{.failure = HidIoFailure::Io, .systemError = systemError};
}

} // namespace

shared_kernel::Result<std::unique_ptr<HidrawDevice>> HidrawDevice::open(const std::filesystem::path& path)
{
    const int fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        const int error = errno;
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   path.string() + ": " + std::generic_category().message(error));
    }
    return std::make_unique<HidrawDevice>(fd);
}

HidrawDevice::~HidrawDevice()
{
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

std::expected<std::size_t, HidIoError> HidrawDevice::write(std::span<const std::uint8_t> report)
{
    for (;;) {
        const ssize_t written = ::write(fd_, report.data(), report.size());
        if (written >= 0) {
            return static_cast<std::size_t>(written);
        }
        if (errno != EINTR) {
            return std::unexpected(ioError(errno));
        }
    }
}

std::expected<std::size_t, HidIoError> HidrawDevice::read(std::span<std::uint8_t> buffer,
                                                          std::chrono::milliseconds timeout)
{
    const auto waitMs = static_cast<int>(
        std::clamp<std::chrono::milliseconds::rep>(timeout.count(), 0, std::numeric_limits<int>::max()));
    pollfd descriptor{.fd = fd_, .events = POLLIN, .revents = 0};
    for (;;) {
        const int ready = ::poll(&descriptor, 1, waitMs);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected(ioError(errno));
        }
        if (ready == 0) {
            return std::unexpected(HidIoError{.failure = HidIoFailure::TimedOut});
        }
        if ((descriptor.revents & POLLIN) == 0) {
            return std::unexpected(ioError(0)); // POLLERR/POLLHUP/POLLNVAL sem dados
        }
        // poll garantiu dados: o read abaixo não bloqueia.
        const ssize_t got = ::read(fd_, buffer.data(), buffer.size());
        if (got > 0) {
            return static_cast<std::size_t>(got);
        }
        if (got == 0) {
            return std::unexpected(ioError(0)); // fim de arquivo / par fechado
        }
        if (errno != EINTR && errno != EAGAIN) {
            return std::unexpected(ioError(errno));
        }
    }
}

} // namespace ghub::lighting::infrastructure
