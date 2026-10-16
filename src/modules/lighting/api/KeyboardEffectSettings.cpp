#include "modules/lighting/api/KeyboardEffectSettings.hpp"

#include "modules/lighting/domain/EffectParameters.hpp"

#include <array>
#include <string_view>

namespace ghub::lighting {

namespace {

using shared_kernel::ErrorCode;

constexpr std::array<std::string_view, 5> kKinds{"wave", "cycle", "breathing", "ripple", "static"};

bool channelInRange(int value)
{
    return value >= 0 && value <= 255;
}

} // namespace

shared_kernel::Result<KeyboardEffectValue> KeyboardEffectSettings::apply(const KeyboardEffectValue& value)
{
    bool known = false;
    for (const std::string_view kind : kKinds) {
        known = known || value.kind == kind;
    }
    if (!known) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "tipo de efeito do teclado desconhecido");
    }
    if (!channelInRange(value.red) || !channelInRange(value.green) || !channelInRange(value.blue)) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "cor do teclado fora de 0-255");
    }
    if (value.brightness < 0 || !domain::EffectBrightness::create(static_cast<std::uint32_t>(value.brightness))) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "brilho do efeito fora de 1-100");
    }
    if (auto accepted = applier_->applyEffect(value); !accepted) {
        return std::unexpected(accepted.error());
    }
    remember(value);
    return value;
}

std::optional<KeyboardEffectValue> KeyboardEffectSettings::last() const
{
    const std::scoped_lock lock{mutex_};
    return last_;
}

void KeyboardEffectSettings::remember(const KeyboardEffectValue& value)
{
    const std::scoped_lock lock{mutex_};
    last_ = value;
}

} // namespace ghub::lighting
