#pragma once

#include "modules/device_access/domain/LeaseRequest.hpp"

#include <chrono>
#include <string>

namespace ghub::device_access::test {

inline LeaseRequest request(const std::string& interfaceId,
                            shared_kernel::Duration maxDuration = std::chrono::seconds{5})
{
    return LeaseRequest{
        .interfaceId = PhysicalInterfaceId::create(interfaceId).value(),
        .holder = LeaseHolder::create("test.holder").value(),
        .purpose = "teste",
        .maxDuration = maxDuration,
    };
}

} // namespace ghub::device_access::test
