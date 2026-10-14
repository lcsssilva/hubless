#include "modules/pointer/domain/LedState.hpp"

#include <algorithm>
#include <string>

namespace ghub::pointer {

namespace {

constexpr std::uint32_t maxChannel = 255;

shared_kernel::Result<LedState> invalid(const LedStateSpec& spec, const std::string& rule)
{
    return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                               "LED " + std::to_string(spec.index) + ": " + rule);
}

bool hasRepetition(const std::vector<LedMode>& modes)
{
    return std::ranges::any_of(modes, [&modes](LedMode mode) { return std::ranges::count(modes, mode) > 1; });
}

} // namespace

LedMode ledModeFromCode(std::uint32_t code) noexcept
{
    switch (code) {
    case 0:
        return LedMode::Off;
    case 1:
        return LedMode::On;
    case 2:
        return LedMode::Cycle;
    case 3:
        return LedMode::Breathing;
    default:
        return LedMode::Unknown;
    }
}

shared_kernel::Result<LedState> LedState::create(LedStateSpec spec)
{
    if (spec.red > maxChannel || spec.green > maxChannel || spec.blue > maxChannel) {
        return invalid(spec, "canal de cor fora de 0-255");
    }
    if (spec.brightness > maxChannel) {
        return invalid(spec, "brilho fora de 0-255");
    }
    if (std::ranges::find(spec.supportedModes, LedMode::Unknown) != spec.supportedModes.end()) {
        return invalid(spec, "modo desconhecido não é um modo suportado");
    }
    if (hasRepetition(spec.supportedModes)) {
        return invalid(spec, "modos suportados repetidos");
    }
    return LedState{std::move(spec)};
}

} // namespace ghub::pointer
