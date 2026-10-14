#include "modules/lighting/domain/KeyColorMap.hpp"

#include <string>

namespace ghub::lighting::domain {

shared_kernel::Result<KeyColorMap> KeyColorMap::create(const std::vector<std::pair<KeyId, RgbColor>>& entries)
{
    KeyColorMap map;
    for (const auto& [key, color] : entries) {
        if (!map.entries_.emplace(key, color).second) {
            return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                       "tecla repetida: " + std::string(G915KeyCatalog::definition(key).name));
        }
    }
    return map;
}

KeyColorMap KeyColorMap::with(KeyId key, RgbColor color) const
{
    KeyColorMap copy = *this;
    copy.entries_.insert_or_assign(key, color);
    return copy;
}

std::optional<RgbColor> KeyColorMap::colorOf(KeyId key) const
{
    const auto found = entries_.find(key);
    if (found == entries_.end()) {
        return std::nullopt;
    }
    return found->second;
}

} // namespace ghub::lighting::domain
