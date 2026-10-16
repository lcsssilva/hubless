#include "modules/lighting/domain/StoredEffect.hpp"

#include <algorithm>

namespace ghub::lighting::domain {

namespace {

// Posições dentro da entrada de 11 bytes (índice 0 = efeito; p_k = índice k + 1).
constexpr std::size_t kId = 0;
constexpr std::size_t kP0 = 1;

constexpr std::uint8_t kStaticId = 0x01;
constexpr std::uint8_t kCycleId = 0x03;
constexpr std::uint8_t kWaveId = 0x04;
constexpr std::uint8_t kBreathingId = 0x0A;
constexpr std::uint8_t kRippleId = 0x0B;

constexpr std::uint8_t kStaticMarker = 0x02;
constexpr std::uint8_t kBreathingForm = 0x01;
constexpr std::uint8_t kWaveDirection = 0x01;
constexpr std::uint32_t kDimmedPercent = 50;

constexpr std::size_t p(std::size_t index)
{
    return kP0 + index;
}

constexpr std::uint8_t high(std::uint32_t value) noexcept
{
    return static_cast<std::uint8_t>(value / 256U % 256U);
}
constexpr std::uint8_t low(std::uint32_t value) noexcept
{
    return static_cast<std::uint8_t>(value % 256U);
}
constexpr std::uint32_t join(std::uint8_t highByte, std::uint8_t lowByte) noexcept
{
    return (static_cast<std::uint32_t>(highByte) << 8U) | lowByte;
}

RgbColor colorOf(const StoredEffectBytes& entry)
{
    return {entry.at(p(0)), entry.at(p(1)), entry.at(p(2))};
}

void setColor(StoredEffectBytes& entry, const RgbColor& color)
{
    entry.at(p(0)) = color.red();
    entry.at(p(1)) = color.green();
    entry.at(p(2)) = color.blue();
}

// Campos editáveis do tipo, escritos sobre `entry` (os demais bytes não são tocados). O ID do efeito também é escrito.
void writeFields(StoredEffectBytes& entry, const KeyboardEffect& effect)
{
    const auto periodMs = effect.period().transform([](auto v) { return v.milliseconds(); }).value_or(0);
    const auto percent = effect.brightness().transform([](auto v) { return v.percent(); }).value_or(0);
    const auto color = effect.color().value_or(RgbColor{0, 0, 0});
    switch (effect.kind()) {
    case KeyboardEffectKind::Off:
        break; // recusado antes de chegar aqui
    case KeyboardEffectKind::Static:
        entry.at(kId) = kStaticId;
        setColor(entry, color);
        entry.at(p(3)) = kStaticMarker;
        break;
    case KeyboardEffectKind::Breathing:
        entry.at(kId) = kBreathingId;
        setColor(entry, color);
        entry.at(p(3)) = high(periodMs);
        entry.at(p(4)) = low(periodMs);
        entry.at(p(5)) = kBreathingForm;
        entry.at(p(6)) = static_cast<std::uint8_t>(percent);
        break;
    case KeyboardEffectKind::Cycle:
        entry.at(kId) = kCycleId;
        entry.at(p(5)) = high(periodMs);
        entry.at(p(6)) = low(periodMs);
        entry.at(p(7)) = static_cast<std::uint8_t>(percent);
        break;
    case KeyboardEffectKind::Wave:
        entry.at(kId) = kWaveId;
        entry.at(p(6)) = low(periodMs);
        entry.at(p(7)) = kWaveDirection;
        entry.at(p(8)) = static_cast<std::uint8_t>(percent);
        entry.at(p(9)) = high(periodMs);
        break;
    case KeyboardEffectKind::Ripple:
        entry.at(kId) = kRippleId;
        setColor(entry, color);
        entry.at(p(5)) = low(effect.rippleSpeed().transform([](auto v) { return v.value(); }).value_or(0));
        break;
    }
}

bool hasBrightness(KeyboardEffectKind kind) noexcept
{
    return kind == KeyboardEffectKind::Breathing || kind == KeyboardEffectKind::Cycle ||
           kind == KeyboardEffectKind::Wave;
}

KeyboardEffect withBrightness(const KeyboardEffect& effect, EffectBrightness brightness)
{
    const auto periodOr = [&effect] {
        return effect.period().value_or(EffectPeriod::create(EffectPeriod::kMinMilliseconds).value());
    };
    switch (effect.kind()) {
    case KeyboardEffectKind::Breathing:
        return KeyboardEffect::breathing(effect.color().value_or(RgbColor{0, 0, 0}), periodOr(), brightness);
    case KeyboardEffectKind::Cycle:
        return KeyboardEffect::cycle(periodOr(), brightness);
    case KeyboardEffectKind::Wave:
        return KeyboardEffect::wave(periodOr(), brightness);
    case KeyboardEffectKind::Off:
    case KeyboardEffectKind::Static:
    case KeyboardEffectKind::Ripple:
        break;
    }
    return effect;
}

} // namespace

std::optional<KeyboardEffect> decodeStoredEffect(const StoredEffectBytes& entry)
{
    switch (entry.at(kId)) {
    case kStaticId:
        if (entry.at(p(3)) != kStaticMarker) {
            return std::nullopt;
        }
        return KeyboardEffect::staticColor(colorOf(entry));
    case kBreathingId: {
        if (entry.at(p(5)) != kBreathingForm) {
            return std::nullopt;
        }
        const auto period = EffectPeriod::create(join(entry.at(p(3)), entry.at(p(4))));
        const auto brightness = EffectBrightness::create(entry.at(p(6)));
        if (!period || !brightness) {
            return std::nullopt;
        }
        return KeyboardEffect::breathing(colorOf(entry), *period, *brightness);
    }
    case kCycleId: {
        const auto period = EffectPeriod::create(join(entry.at(p(5)), entry.at(p(6))));
        const auto brightness = EffectBrightness::create(entry.at(p(7)));
        if (!period || !brightness) {
            return std::nullopt;
        }
        return KeyboardEffect::cycle(*period, *brightness);
    }
    case kWaveId: {
        if (entry.at(p(7)) != kWaveDirection) {
            return std::nullopt;
        }
        const auto period = EffectPeriod::create(join(entry.at(p(9)), entry.at(p(6))));
        const auto brightness = EffectBrightness::create(entry.at(p(8)));
        if (!period || !brightness) {
            return std::nullopt;
        }
        return KeyboardEffect::wave(*period, *brightness);
    }
    case kRippleId: {
        const auto speed = RippleSpeed::create(entry.at(p(5)));
        if (!speed) {
            return std::nullopt;
        }
        return KeyboardEffect::ripple(colorOf(entry), *speed);
    }
    default:
        return std::nullopt;
    }
}

shared_kernel::Result<StoredEffectBytes> encodeStoredEffect(const KeyboardEffect& wanted,
                                                            const StoredEffectBytes& original)
{
    if (wanted.kind() == KeyboardEffectKind::Off) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "o efeito desligado não tem formato gravado; use cor fixa preta");
    }
    const auto current = decodeStoredEffect(original);
    StoredEffectBytes result{}; // molde: zeros
    if (current.has_value() && current->kind() == wanted.kind()) {
        result = original; // mesmo tipo: só os campos editáveis mudam
    }
    writeFields(result, wanted);
    return result;
}

StoredEffectBytes deriveDimmedVariant(const StoredEffectBytes& oldPrimary, const StoredEffectBytes& oldVariant,
                                      const StoredEffectBytes& newPrimary)
{
    if (newPrimary == oldPrimary) {
        return oldVariant;
    }
    const auto wanted = decodeStoredEffect(newPrimary);
    if (!wanted.has_value()) {
        return oldVariant; // primária bruta: a variante não é tocada
    }
    const auto previousVariant = decodeStoredEffect(oldVariant);
    const auto previousPrimary = decodeStoredEffect(oldPrimary);
    const bool sameKind = previousVariant.has_value() && previousPrimary.has_value() &&
                          previousVariant->kind() == wanted->kind() && previousPrimary->kind() == wanted->kind();
    KeyboardEffect forVariant = *wanted;
    StoredEffectBytes base{}; // molde
    if (sameKind) {
        base = oldVariant;
        if (previousVariant->brightness().has_value()) {
            forVariant = withBrightness(*wanted, *previousVariant->brightness());
        }
    } else if (hasBrightness(wanted->kind())) {
        forVariant = withBrightness(*wanted, EffectBrightness::create(kDimmedPercent).value());
    }
    StoredEffectBytes result = base;
    writeFields(result, forVariant);
    return result;
}

} // namespace ghub::lighting::domain
