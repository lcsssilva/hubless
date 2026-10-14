#include "modules/profiles/domain/ApplicationId.hpp"

namespace ghub::profiles {

namespace {

bool isAsciiSpace(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

} // namespace

shared_kernel::Result<ApplicationId> ApplicationId::create(std::string_view raw)
{
    using shared_kernel::ErrorCode;
    while (!raw.empty() && isAsciiSpace(raw.front())) {
        raw.remove_prefix(1);
    }
    while (!raw.empty() && isAsciiSpace(raw.back())) {
        raw.remove_suffix(1);
    }
    if (raw.empty() || raw.size() > maximumLength) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "aplicativo deve ter de 1 a 128 caracteres");
    }
    std::string value;
    value.reserve(raw.size());
    for (const char c : raw) {
        if (c >= 'A' && c <= 'Z') {
            value.push_back(static_cast<char>(c - 'A' + 'a'));
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
            value.push_back(c);
        } else {
            return shared_kernel::fail(ErrorCode::InvalidArgument, "aplicativo só aceita letras, dígitos, . _ -");
        }
    }
    return ApplicationId{std::move(value)};
}

ApplicationId ApplicationId::ownApplication()
{
    return ApplicationId{"ghub-linux"};
}

} // namespace ghub::profiles
