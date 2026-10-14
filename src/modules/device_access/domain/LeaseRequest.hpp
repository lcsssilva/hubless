#pragma once

#include "modules/device_access/domain/PhysicalInterfaceId.hpp"
#include "shared_kernel/Clock.hpp"
#include "shared_kernel/Result.hpp"

#include <string>

namespace ghub::device_access {

// Módulo/adaptador que segura um lease (ex.: "pointer.ratbag"). Objeto de valor, nunca vazio.
class LeaseHolder {
public:
    [[nodiscard]] static shared_kernel::Result<LeaseHolder> create(std::string name);

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    friend bool operator==(const LeaseHolder&, const LeaseHolder&) = default;

private:
    explicit LeaseHolder(std::string name) : name_(std::move(name)) {}

    std::string name_;
};

// Pedido de acesso exclusivo. Validado por AccessRegistry::acquire.
struct LeaseRequest {
    PhysicalInterfaceId interfaceId;
    LeaseHolder holder;
    std::string purpose;
    shared_kernel::Duration maxDuration;
};

} // namespace ghub::device_access
