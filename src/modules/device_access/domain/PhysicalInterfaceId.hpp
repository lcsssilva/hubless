#pragma once

#include "shared_kernel/Result.hpp"

#include <compare>
#include <string>

namespace ghub::device_access {

// Canal de comando exclusivo: nó hidraw, objeto D-Bus, /dev/videoN ou nó PipeWire
// (DEVICE_ACCESS_POLICY.md §1). Objeto de valor imutável, nunca vazio.
class PhysicalInterfaceId {
public:
    [[nodiscard]] static shared_kernel::Result<PhysicalInterfaceId> create(std::string value);

    [[nodiscard]] const std::string& value() const noexcept { return value_; }

    friend auto operator<=>(const PhysicalInterfaceId&, const PhysicalInterfaceId&) = default;

private:
    explicit PhysicalInterfaceId(std::string value) : value_(std::move(value)) {}

    std::string value_;
};

} // namespace ghub::device_access
