#pragma once

#include "shared_kernel/DomainError.hpp"

#include <expected>
#include <string>
#include <utility>

namespace ghub::shared_kernel {

template <typename T> using Result = std::expected<T, DomainError>;

[[nodiscard]] inline std::unexpected<DomainError> fail(ErrorCode code, std::string detail = {})
{
    return std::unexpected(DomainError{.code = code, .detail = std::move(detail)});
}

} // namespace ghub::shared_kernel
