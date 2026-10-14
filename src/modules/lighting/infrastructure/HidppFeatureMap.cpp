#include "modules/lighting/infrastructure/HidppFeatureMap.hpp"

namespace ghub::lighting::infrastructure {

std::optional<std::uint8_t> HidppFeatureMap::find(std::uint8_t deviceIndex, std::uint16_t featureId) const
{
    const auto it = entries_.find({deviceIndex, featureId});
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void HidppFeatureMap::record(std::uint8_t deviceIndex, std::uint16_t featureId, std::uint8_t featureIndex)
{
    entries_[{deviceIndex, featureId}] = featureIndex;
}

} // namespace ghub::lighting::infrastructure
