#pragma once

#include "modules/device_access/api/ExternalProcessProbe.hpp"

#include <filesystem>

namespace ghub::device_access {

// Adaptador do port sobre /proc: lê somente <pid>/comm de cada processo, nunca cmdline nem environ.
class ProcExternalProcessProbe final : public ExternalProcessProbe {
public:
    explicit ProcExternalProcessProbe(std::filesystem::path procRoot = "/proc");

    [[nodiscard]] shared_kernel::Result<std::vector<ExternalOwner>> runningOwners() override;

private:
    std::filesystem::path procRoot_;
};

} // namespace ghub::device_access
