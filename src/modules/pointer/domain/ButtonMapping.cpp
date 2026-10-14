#include "modules/pointer/domain/ButtonMapping.hpp"

#include <algorithm>
#include <string>

namespace ghub::pointer {

namespace {

shared_kernel::Result<ButtonMapping> invalid(const ButtonMappingSpec& spec, const std::string& rule)
{
    return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                               "botão " + std::to_string(spec.index) + ": " + rule);
}

bool hasRepetition(const std::vector<ButtonActionKind>& kinds)
{
    return std::ranges::any_of(kinds, [&kinds](ButtonActionKind kind) { return std::ranges::count(kinds, kind) > 1; });
}

} // namespace

shared_kernel::Result<ButtonMapping> ButtonMapping::create(ButtonMappingSpec spec)
{
    if (spec.action.kind() == ButtonActionKind::Button && spec.action.value() == 0) {
        return invalid(spec, "o número do botão atribuído começa em 1");
    }
    if (std::ranges::find(spec.supportedKinds, ButtonActionKind::Unknown) != spec.supportedKinds.end()) {
        return invalid(spec, "tipo desconhecido não é um tipo suportado");
    }
    if (hasRepetition(spec.supportedKinds)) {
        return invalid(spec, "tipos suportados repetidos");
    }
    return ButtonMapping{std::move(spec)};
}

} // namespace ghub::pointer
