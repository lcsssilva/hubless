#include "modules/device_access/infrastructure/ProcExternalProcessProbe.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <utility>

namespace ghub::device_access {

namespace {

namespace fs = std::filesystem;

using shared_kernel::ErrorCode;
using shared_kernel::fail;

// O kernel limita comm a 16 bytes; 64 já é folga e impede ler conteúdo arbitrário.
constexpr std::size_t maxCommBytes = 64;

bool isPidDirectory(const std::string& name)
{
    return !name.empty() && std::ranges::all_of(name, [](unsigned char c) { return std::isdigit(c) != 0; });
}

// Conteúdo de <pid>/comm sem o '\n' final; vazio se o processo terminou ou o arquivo é ilegível.
std::string readComm(const fs::path& processDirectory)
{
    std::ifstream stream(processDirectory / "comm", std::ios::binary);
    if (!stream) {
        return {};
    }
    std::string content(maxCommBytes, '\0');
    stream.read(content.data(), static_cast<std::streamsize>(content.size()));
    content.resize(static_cast<std::size_t>(stream.gcount()));
    if (!content.empty() && content.back() == '\n') {
        content.pop_back();
    }
    return content;
}

} // namespace

ProcExternalProcessProbe::ProcExternalProcessProbe(fs::path procRoot) : procRoot_(std::move(procRoot)) {}

shared_kernel::Result<std::vector<ExternalOwner>> ProcExternalProcessProbe::runningOwners()
{
    std::error_code error;
    fs::directory_iterator it(procRoot_, error);
    if (error) {
        return fail(ErrorCode::BackendUnavailable, procRoot_.string() + ": " + error.message());
    }

    std::vector<ExternalOwner> owners;
    for (const fs::directory_iterator end; it != end; it.increment(error)) {
        const auto name = it->path().filename().string();
        if (!isPidDirectory(name)) {
            continue;
        }
        if (const auto owner = externalOwnerForProcessName(readComm(it->path()))) {
            owners.push_back(*owner);
        }
    }
    if (error) {
        return fail(ErrorCode::BackendUnavailable, procRoot_.string() + ": " + error.message());
    }

    std::ranges::sort(owners);
    owners.erase(std::ranges::unique(owners).begin(), owners.end());
    return owners;
}

} // namespace ghub::device_access
