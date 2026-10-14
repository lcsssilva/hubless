#include "modules/device_access/domain/ExternalOwner.hpp"

#include <array>
#include <utility>

namespace ghub::device_access {

namespace {

constexpr std::array<std::pair<ExternalOwner, std::string_view>, 5> knownOwners{
    {
        {ExternalOwner::OpenRgb, "openrgb"},
        {ExternalOwner::Solaar, "solaar"},
        {ExternalOwner::Piper, "piper"},
        {ExternalOwner::Ratbagd, "ratbagd"},
        {ExternalOwner::HeadsetControl, "headsetcontrol"},
    },
};

} // namespace

std::optional<ExternalOwner> externalOwnerForProcessName(std::string_view comm) noexcept
{
    for (const auto& [owner, name] : knownOwners) {
        if (comm == name) {
            return owner;
        }
    }
    return std::nullopt;
}

std::string_view processName(ExternalOwner owner) noexcept
{
    for (const auto& [known, name] : knownOwners) {
        if (known == owner) {
            return name;
        }
    }
    return {};
}

} // namespace ghub::device_access
