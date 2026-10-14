#include "modules/camera/infrastructure/LinuxV4l2Api.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <limits>

namespace ghub::camera {

int LinuxV4l2Api::open(const std::filesystem::path& node)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): ::open é variádica na libc; não há alternativa.
    const int fd = ::open(node.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    return fd >= 0 ? fd : -errno;
}

int LinuxV4l2Api::ioctl(int fd, unsigned long request, void* argument)
{
    for (;;) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): ::ioctl é variádica na libc; não há alternativa.
        if (::ioctl(fd, request, argument) == 0) {
            return 0;
        }
        if (errno != EINTR) {
            return -errno;
        }
    }
}

void LinuxV4l2Api::close(int fd) noexcept
{
    ::close(fd);
}

int LinuxV4l2Api::openForStreaming(const std::filesystem::path& node)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): ::open é variádica na libc; não há alternativa.
    const int fd = ::open(node.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    return fd >= 0 ? fd : -errno;
}

int LinuxV4l2Api::mapBuffer(int fd, std::size_t length, std::uint32_t offset, const std::byte*& address)
{
    const void* const mapped = ::mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, static_cast<off_t>(offset));
    if (mapped ==
        MAP_FAILED) { // NOLINT(cppcoreguidelines-pro-type-cstyle-cast,performance-no-int-to-ptr): macro da libc.
        return -errno;
    }
    address = static_cast<const std::byte*>(mapped);
    return 0;
}

void LinuxV4l2Api::unmapBuffer(const std::byte* address, std::size_t length) noexcept
{
    // O endereço veio de mapBuffer, que o recebeu de mmap como void*; devolvê-lo a munmap é o par correto.
    ::munmap(const_cast<std::byte*>(address), length); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

int LinuxV4l2Api::poll(int fd, std::chrono::milliseconds timeout)
{
    pollfd descriptor{.fd = fd, .events = POLLIN, .revents = 0};
    const auto milliseconds =
        std::clamp<std::chrono::milliseconds::rep>(timeout.count(), 0, std::numeric_limits<int>::max());
    const int ready = ::poll(&descriptor, 1, static_cast<int>(milliseconds));
    if (ready < 0) {
        return errno == EINTR ? 0 : -errno; // interrompido: conta como prazo esgotado; o chamador tenta de novo
    }
    return ready == 0 ? 0 : descriptor.revents;
}

} // namespace ghub::camera
