#include "modules/lighting/infrastructure/G915OnboardPolicy.hpp"

#include "modules/lighting/domain/OnboardSlot.hpp"

#include <algorithm>

namespace ghub::lighting::infrastructure {

namespace {

constexpr std::uint16_t kRootFeature = 0x0000;
constexpr std::uint16_t kFeatureSetFeature = 0x0001;
constexpr std::uint16_t kOnboardFeature = 0x8100;
constexpr std::size_t kAddressWriteParams = 6;
constexpr std::size_t kWriteBlockSize = 16;
// Tamanho do setor do G915 (sector_size 255, captura C-06): BE16 0x00ff nos dois últimos bytes de fn6.
constexpr std::uint8_t kSectorSizeHigh = 0x00;
constexpr std::uint8_t kSectorSizeLow = 0xFF;

} // namespace

bool G915OnboardPolicy::isAllowed(std::uint16_t featureId, std::uint8_t function,
                                  std::span<const std::uint8_t> params) const noexcept
{
    if (featureId == kRootFeature || featureId == kFeatureSetFeature) {
        return function == 0 || function == 1;
    }
    if (featureId != kOnboardFeature) {
        return false;
    }
    switch (function) {
    case 0:
    case 2:
    case 4:
    case 5:
        return true;
    case 6: {
        if (params.size() != kAddressWriteParams) {
            return false;
        }
        const auto sector = (static_cast<unsigned>(params[0]) << 8U) | params[1];
        const unsigned limit = std::min<unsigned>(profileCount_, domain::OnboardSlot::kMax);
        const bool addressZero = params[2] == 0 && params[3] == 0;
        const bool fullSector = params[4] == kSectorSizeHigh && params[5] == kSectorSizeLow;
        return sector >= 1 && sector <= limit && addressZero && fullSector;
    }
    case 7:
        return params.size() == kWriteBlockSize;
    case 8:
        return params.empty();
    default:
        return false;
    }
}

} // namespace ghub::lighting::infrastructure
