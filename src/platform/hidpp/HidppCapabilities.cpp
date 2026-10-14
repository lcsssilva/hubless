#include "platform/hidpp/HidppCapabilities.hpp"

namespace ghub::platform::hidpp {

namespace {
constexpr std::uint16_t featureSet = 0x0001;
}

std::expected<std::vector<HidppFeatureEntry>, HidppError> HidppCapabilities::enumerateFeatures(std::uint8_t deviceIndex)
{
    const auto count = channel_.call(deviceIndex, featureSet, 0);
    if (!count.has_value()) {
        return std::unexpected(count.error());
    }
    std::vector<HidppFeatureEntry> entries;
    for (std::uint8_t index = 1; index <= (*count)[0]; ++index) {
        const std::array<std::uint8_t, 1> params{index};
        const auto entry = channel_.call(deviceIndex, featureSet, 1, params);
        if (!entry.has_value()) {
            return std::unexpected(entry.error());
        }
        entries.push_back(HidppFeatureEntry{.index = index,
                                            .featureId = static_cast<std::uint16_t>(((*entry)[0] << 8) | (*entry)[1]),
                                            .type = (*entry)[2]});
    }
    return entries;
}

std::expected<std::optional<HidppChannel::Response>, HidppError> HidppCapabilities::rawInfo(std::uint8_t deviceIndex,
                                                                                            std::uint16_t featureId)
{
    if (featureId != featureEffects && featureId != featurePerKey && featureId != featureColorLed) {
        return std::unexpected(HidppError{.kind = HidppErrorKind::InvalidArgument,
                                          .detail = "GetInfo só é lido nas features de iluminação"});
    }
    const auto info = channel_.call(deviceIndex, featureId, 0);
    if (!info.has_value()) {
        if (info.error().kind == HidppErrorKind::FeatureNotPresent) {
            return std::optional<HidppChannel::Response>{};
        }
        return std::unexpected(info.error());
    }
    return std::optional<HidppChannel::Response>{*info};
}

} // namespace ghub::platform::hidpp
