#pragma once

#include "modules/battery/application/BatterySource.hpp"

#include <utility>
#include <vector>

namespace ghub::fakes {

// Fonte de bateria que devolve o resultado configurado pelo teste.
class FakeBatterySource final : public battery::BatterySource {
public:
    [[nodiscard]] shared_kernel::Result<std::vector<battery::SourcedBatteryReading>> readAll() override
    {
        return result_;
    }

    void setResult(shared_kernel::Result<std::vector<battery::SourcedBatteryReading>> result)
    {
        result_ = std::move(result);
    }

private:
    shared_kernel::Result<std::vector<battery::SourcedBatteryReading>> result_;
};

} // namespace ghub::fakes
