#include "modules/pointer/domain/ResolutionStage.hpp"

#include <algorithm>
#include <string>

namespace ghub::pointer {

namespace {

shared_kernel::Result<ResolutionStage> invalid(const ResolutionStageSpec& spec, const std::string& rule)
{
    return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                               "estágio " + std::to_string(spec.index) + ": " + rule);
}

} // namespace

shared_kernel::Result<ResolutionStage> ResolutionStage::create(ResolutionStageSpec spec)
{
    if (spec.dpiX == 0) {
        return invalid(spec, "DPI X deve ser positivo");
    }
    if (spec.dpiY.has_value() && *spec.dpiY == 0) {
        return invalid(spec, "DPI Y deve ser positivo");
    }
    const auto& list = spec.supportedDpis;
    if (std::ranges::find(list, 0U) != list.end()) {
        return invalid(spec, "DPI suportado deve ser positivo");
    }
    if (std::ranges::adjacent_find(list, std::ranges::greater_equal{}) != list.end()) {
        return invalid(spec, "DPIs suportados devem ser crescentes e sem repetição");
    }
    return ResolutionStage{std::move(spec)};
}

bool ResolutionStage::isDpiListed() const
{
    const auto listed = [this](std::uint32_t dpi) { return std::ranges::binary_search(spec_.supportedDpis, dpi); };
    return listed(spec_.dpiX) && (!spec_.dpiY.has_value() || listed(*spec_.dpiY));
}

} // namespace ghub::pointer
