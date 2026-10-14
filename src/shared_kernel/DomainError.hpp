#pragma once

#include <cstdint>
#include <string>

namespace ghub::shared_kernel {

// Categorias de falha esperada. Valores novos entram somente com um caso de uso que os produza.
enum class ErrorCode : std::uint8_t {
    InvalidArgument,
    DeviceBusy,
    LeaseExpired,
    LeaseRevoked,
    BackendUnavailable,
    MalformedData,
};

struct DomainError {
    ErrorCode code;
    std::string detail;

    friend bool operator==(const DomainError&, const DomainError&) = default;
};

} // namespace ghub::shared_kernel
