#include "modules/pointer/domain/PointerProfile.hpp"

#include <algorithm>
#include <string>

namespace ghub::pointer {

namespace {

shared_kernel::Result<PointerProfile> invalid(const PointerProfileSpec& spec, const std::string& rule)
{
    return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                               "perfil " + std::to_string(spec.index) + ": " + rule);
}

template <typename Item> bool hasRepeatedIndex(const std::vector<Item>& items)
{
    return std::ranges::any_of(
        items, [&items](const Item& item) { return std::ranges::count(items, item.index(), &Item::index) > 1; });
}

} // namespace

shared_kernel::Result<PointerProfile> PointerProfile::create(PointerProfileSpec spec)
{
    const auto& rates = spec.supportedReportRates;
    if (std::ranges::find(rates, 0U) != rates.end()) {
        return invalid(spec, "taxa de report suportada deve ser positiva");
    }
    if (std::ranges::any_of(rates, [&rates](std::uint32_t rate) { return std::ranges::count(rates, rate) > 1; })) {
        return invalid(spec, "taxas de report suportadas repetidas");
    }
    if (hasRepeatedIndex(spec.stages)) {
        return invalid(spec, "índice de estágio repetido");
    }
    if (hasRepeatedIndex(spec.buttons)) {
        return invalid(spec, "índice de botão repetido");
    }
    if (hasRepeatedIndex(spec.leds)) {
        return invalid(spec, "índice de LED repetido");
    }
    return PointerProfile{std::move(spec)};
}

} // namespace ghub::pointer
