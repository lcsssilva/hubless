#include "modules/pointer/infrastructure/FileOnboardBackupStore.hpp"

#include <cerrno>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ghub::pointer::infrastructure {

namespace {

shared_kernel::DomainError unavailable(const std::string& what, int error)
{
    return {
        .code = shared_kernel::ErrorCode::BackendUnavailable,
        .detail = what + ": " + std::generic_category().message(error),
    };
}

} // namespace

std::filesystem::path FileOnboardBackupStore::pathFor(std::uint16_t sector) const
{
    return directory_ / std::format("g703-4086-sector-{:04x}.bin", static_cast<unsigned>(sector));
}

shared_kernel::Result<std::optional<std::vector<std::uint8_t>>> FileOnboardBackupStore::load(std::uint16_t sector)
{
    const auto path = pathFor(sector);
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if (status.type() == std::filesystem::file_type::not_found) {
        return std::optional<std::vector<std::uint8_t>>{};
    }
    if (error || !std::filesystem::is_regular_file(status)) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   "cópia de segurança ilegível: " + error.message());
    }
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, error.message());
    }
    if (size > maxBackupSize) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData, "cópia de segurança maior que 4096 bytes");
    }
    std::ifstream in{path, std::ios::binary};
    if (!in) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "não foi possível abrir a cópia");
    }
    std::vector<std::uint8_t> data{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    return std::optional<std::vector<std::uint8_t>>{std::move(data)};
}

shared_kernel::Result<void> FileOnboardBackupStore::saveIfAbsent(std::uint16_t sector,
                                                                 std::span<const std::uint8_t> bytes)
{
    if (bytes.empty() || bytes.size() > maxBackupSize) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "tamanho de cópia fora de 1..4096");
    }
    std::error_code error;
    const bool existed = std::filesystem::is_directory(directory_, error);
    std::filesystem::create_directories(directory_, error);
    if (error) {
        return std::unexpected(unavailable("diretório de cópias: " + error.message(), error.value()));
    }
    if (!existed && ::chmod(directory_.c_str(), 0700) != 0) {
        return std::unexpected(unavailable("chmod do diretório de cópias", errno));
    }

    // mkstemp cria com 0600 e O_EXCL no mesmo diretório do destino (rename/link atômico).
    std::string temporary = (directory_ / ".g703-backup-XXXXXX").string();
    const int fd = ::mkstemp(temporary.data());
    if (fd < 0) {
        return std::unexpected(unavailable("arquivo temporário da cópia", errno));
    }
    bool ok = true;
    int failure = 0;
    for (std::size_t done = 0; done < bytes.size() && ok;) {
        const auto written = ::write(fd, bytes.subspan(done).data(), bytes.size() - done);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            ok = false;
            failure = errno;
        } else {
            done += static_cast<std::size_t>(written);
        }
    }
    if (ok && ::fsync(fd) != 0) {
        ok = false;
        failure = errno;
    }
    ::close(fd);
    if (!ok) {
        ::unlink(temporary.c_str());
        return std::unexpected(unavailable("gravação da cópia", failure));
    }

    const auto target = pathFor(sector);
    const int linked = ::link(temporary.c_str(), target.c_str());
    const int linkError = errno;
    ::unlink(temporary.c_str());
    if (linked != 0 && linkError != EEXIST) {
        return std::unexpected(unavailable("publicação da cópia", linkError));
    }
    return {}; // EEXIST: a cópia antiga é mantida, e isso também é sucesso
}

} // namespace ghub::pointer::infrastructure
