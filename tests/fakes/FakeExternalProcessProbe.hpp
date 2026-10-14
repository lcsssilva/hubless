#pragma once

#include "modules/device_access/api/ExternalProcessProbe.hpp"

#include <utility>
#include <vector>

namespace ghub::fakes {

// Sonda de processos externos que devolve o resultado configurado pelo teste e conta as consultas.
class FakeExternalProcessProbe final : public device_access::ExternalProcessProbe {
public:
    [[nodiscard]] shared_kernel::Result<std::vector<device_access::ExternalOwner>> runningOwners() override
    {
        ++calls_;
        return result_;
    }

    void setResult(shared_kernel::Result<std::vector<device_access::ExternalOwner>> result)
    {
        result_ = std::move(result);
    }

    [[nodiscard]] int calls() const noexcept { return calls_; }

private:
    shared_kernel::Result<std::vector<device_access::ExternalOwner>> result_;
    int calls_ = 0;
};

} // namespace ghub::fakes
