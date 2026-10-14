#include "modules/lighting/domain/EffectParameters.hpp"

#include <string>

namespace ghub::lighting::domain {

namespace {

shared_kernel::Result<std::uint32_t> inRange(std::uint32_t value, std::uint32_t min, std::uint32_t max,
                                             const char* what)
{
    if (value < min || value > max) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   std::string(what) + " fora da faixa " + std::to_string(min) + ".." +
                                       std::to_string(max) + ": " + std::to_string(value));
    }
    return value;
}

} // namespace

shared_kernel::Result<EffectPeriod> EffectPeriod::create(std::uint32_t milliseconds)
{
    return inRange(milliseconds, kMinMilliseconds, kMaxMilliseconds, "período (ms)").transform([](std::uint32_t v) {
        return EffectPeriod{v};
    });
}

shared_kernel::Result<EffectBrightness> EffectBrightness::create(std::uint32_t percent)
{
    return inRange(percent, kMinPercent, kMaxPercent, "brilho (%)").transform([](std::uint32_t v) {
        return EffectBrightness{v};
    });
}

shared_kernel::Result<RippleSpeed> RippleSpeed::create(std::uint32_t value)
{
    return inRange(value, kMin, kMax, "velocidade Ripple").transform([](std::uint32_t v) { return RippleSpeed{v}; });
}

} // namespace ghub::lighting::domain
