#include "modules/battery/infrastructure/PassiveReportSource.hpp"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

#include <utility>

namespace ghub::battery {

FdPassiveReportSource::FdPassiveReportSource(UniqueFd fd) noexcept : fd_(std::move(fd)) {}

int FdPassiveReportSource::fd() const
{
    return fd_.get();
}

std::expected<std::size_t, int> FdPassiveReportSource::readReport(std::span<std::uint8_t> buffer)
{
    const ssize_t n = ::read(fd_.get(), buffer.data(), buffer.size());
    if (n > 0) {
        return static_cast<std::size_t>(n);
    }
    if (n == 0) {
        return std::unexpected(ENODEV); // EOF: o nó foi removido
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        return std::size_t{0};
    }
    return std::unexpected(errno);
}

std::expected<std::unique_ptr<PassiveReportSource>, int> openReadOnlyReportSource(const std::filesystem::path& path)
{
    // Somente leitura: este caminho nunca abre o nó com permissão de escrita.
    UniqueFd fd{::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC)};
    if (!fd.valid()) {
        return std::unexpected(errno);
    }
    return std::make_unique<FdPassiveReportSource>(std::move(fd));
}

} // namespace ghub::battery
