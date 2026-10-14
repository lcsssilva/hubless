#pragma once

#include "modules/device_access/domain/ExternalOwner.hpp"
#include "shared_kernel/Result.hpp"

#include <vector>

namespace ghub::device_access {

// Port: descobre quais softwares externos conhecidos estão em execução. Só observa processos; a
// consulta ativa a dispositivo conta como escrita e não é permitida aqui (DEVICE_ACCESS_POLICY §1/§3).
class ExternalProcessProbe {
public:
    virtual ~ExternalProcessProbe() = default;
    [[nodiscard]] virtual shared_kernel::Result<std::vector<ExternalOwner>> runningOwners() = 0;

protected:
    ExternalProcessProbe() = default;
    ExternalProcessProbe(const ExternalProcessProbe&) = default;
    ExternalProcessProbe& operator=(const ExternalProcessProbe&) = default;
    ExternalProcessProbe(ExternalProcessProbe&&) = default;
    ExternalProcessProbe& operator=(ExternalProcessProbe&&) = default;
};

} // namespace ghub::device_access
