#pragma once

#include "modules/device_inventory/application/IdentitySource.hpp"

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace ghub::fakes {

// Fonte de identidade que registra os alvos lidos e devolve o resultado configurado. Seguro para ser chamada de
// uma thread de trabalho. `onRead` (opcional) roda dentro de cada leitura, antes de devolver (ex.: para bloquear a
// thread até o teste liberar ou para inspecionar o registro de leases durante a leitura).
class FakeIdentitySource final : public device_inventory::IdentitySource {
public:
    [[nodiscard]] shared_kernel::Result<device_inventory::DeviceIdentity>
    read(device_inventory::IdentityTarget target) override
    {
        std::function<void()> hook;
        {
            const std::scoped_lock lock{mutex_};
            targets_.push_back(target);
            hook = onRead_;
        }
        if (hook) {
            hook();
        }
        const std::scoped_lock lock{mutex_};
        return result_;
    }

    void setResult(shared_kernel::Result<device_inventory::DeviceIdentity> result)
    {
        const std::scoped_lock lock{mutex_};
        result_ = std::move(result);
    }

    void setOnRead(std::function<void()> hook)
    {
        const std::scoped_lock lock{mutex_};
        onRead_ = std::move(hook);
    }

    [[nodiscard]] std::vector<device_inventory::IdentityTarget> targets() const
    {
        const std::scoped_lock lock{mutex_};
        return targets_;
    }

private:
    mutable std::mutex mutex_;
    shared_kernel::Result<device_inventory::DeviceIdentity> result_{device_inventory::DeviceIdentity{}};
    std::function<void()> onRead_;
    std::vector<device_inventory::IdentityTarget> targets_;
};

} // namespace ghub::fakes
