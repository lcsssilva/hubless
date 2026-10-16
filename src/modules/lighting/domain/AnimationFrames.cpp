#include "modules/lighting/domain/AnimationFrames.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

namespace ghub::lighting::domain {

namespace {

constexpr std::int64_t kWheelSteps = 1536; // 6 setores de 256 passos
constexpr std::int64_t kStarPeriodMs = 2000;
constexpr RgbColor kBlack{0, 0, 0};

std::int64_t floorMod(std::int64_t value, std::int64_t modulus)
{
    const auto remainder = value % modulus;
    return remainder < 0 ? remainder + modulus : remainder;
}

std::uint8_t channel(std::int64_t value)
{
    return static_cast<std::uint8_t>(std::clamp<std::int64_t>(value, 0, 255));
}

// Roda de matiz com saturação e valor máximos; `position` em 0..1535 (já reduzida).
RgbColor wheelColor(std::int64_t position)
{
    const auto sector = position / 256;
    const auto f = position % 256;
    switch (sector) {
    case 0:
        return {255, channel(f), 0};
    case 1:
        return {channel(255 - f), 255, 0};
    case 2:
        return {0, 255, channel(f)};
    case 3:
        return {0, channel(255 - f), 255};
    case 4:
        return {channel(f), 0, 255};
    default:
        return {255, 0, channel(255 - f)};
    }
}

std::int64_t wheelPositionAt(std::int64_t timeMs, std::uint32_t periodMs)
{
    const std::int64_t period = std::max<std::uint32_t>(periodMs, 1);
    return floorMod(timeMs, period) * kWheelSteps / period;
}

// Constrói o quadro em O(n) com a cor de cada tecla dada por `colorOf(posição)`.
template <typename Fn> KeyColorMap buildFrame(Fn&& colorOf)
{
    std::vector<std::pair<KeyId, RgbColor>> entries;
    entries.reserve(G915KeyCatalog::keys().size());
    for (const auto& key : G915KeyCatalog::keys()) {
        entries.emplace_back(key.id, colorOf(key.id.position()));
    }
    auto map = KeyColorMap::create(entries);
    return map.has_value() ? *std::move(map) : KeyColorMap{}; // chaves do catálogo são únicas: nunca falha
}

// splitmix64: mistura barata e determinística de (seed, tecla).
std::uint64_t mix(std::uint64_t value)
{
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

RgbColor scaled(RgbColor color, std::int64_t per1000)
{
    return {channel(color.red() * per1000 / 1000), channel(color.green() * per1000 / 1000),
            channel(color.blue() * per1000 / 1000)};
}

RgbColor lerp(RgbColor from, RgbColor to, std::int64_t offset, std::int64_t span)
{
    const auto mixChannel = [&](std::uint8_t a, std::uint8_t b) {
        return channel((a * (span - offset) + b * offset + span / 2) / span);
    };
    return {mixChannel(from.red(), to.red()), mixChannel(from.green(), to.green()), mixChannel(from.blue(), to.blue())};
}

} // namespace

shared_kernel::Result<KeyframeTrack> KeyframeTrack::create(std::vector<Keyframe> frames)
{
    if (frames.empty()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "keyframes vazios");
    }
    for (std::size_t i = 1; i < frames.size(); ++i) {
        if (frames[i].atMs <=
            frames[i - 1].atMs) { // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                       "tempos dos keyframes precisam ser estritamente crescentes");
        }
    }
    return KeyframeTrack{std::move(frames)};
}

RgbColor KeyframeTrack::colorAt(std::int64_t timeMs) const
{
    const auto last = static_cast<std::int64_t>(frames_.back().atMs);
    const auto t = last > 0 ? floorMod(timeMs, last) : 0;
    if (t <= static_cast<std::int64_t>(frames_.front().atMs)) {
        return frames_.front().color;
    }
    for (std::size_t i = 1; i < frames_.size(); ++i) {
        const auto& next = frames_[i]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        if (t <= static_cast<std::int64_t>(next.atMs)) {
            const auto& previous =
                frames_[i - 1]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            const auto span = static_cast<std::int64_t>(next.atMs) - static_cast<std::int64_t>(previous.atMs);
            return lerp(previous.color, next.color, t - static_cast<std::int64_t>(previous.atMs), span);
        }
    }
    return frames_.back().color;
}

KeyColorMap colorCycleFrame(std::int64_t timeMs, const ColorCycleParams& params)
{
    const auto color = wheelColor(wheelPositionAt(timeMs, params.periodMs));
    return buildFrame([&](std::size_t) { return color; });
}

KeyColorMap waveFrame(std::int64_t timeMs, const WaveParams& params)
{
    const auto base = wheelPositionAt(timeMs, params.periodMs);
    const auto count = static_cast<std::int64_t>(G915KeyCatalog::keys().size());
    return buildFrame([&](std::size_t position) {
        const auto offset = static_cast<std::int64_t>(position) * kWheelSteps / count;
        const auto shifted = params.direction == WaveDirection::LeftToRight ? base - offset : base + offset;
        return wheelColor(floorMod(shifted, kWheelSteps));
    });
}

KeyColorMap starFrame(std::int64_t timeMs, const StarParams& params)
{
    const auto density = std::min<std::uint64_t>(params.densityPercent, 100);
    return buildFrame([&](std::size_t position) {
        const auto hash = mix((static_cast<std::uint64_t>(params.seed) << 32U) ^ position);
        if (hash % 100 >= density) {
            return kBlack;
        }
        const auto phase = static_cast<std::int64_t>((hash >> 8U) % 20) * 100; // fase em passos de 100 ms
        const auto local = floorMod(timeMs + phase, kStarPeriodMs);
        const auto brightness = local < kStarPeriodMs / 2 ? local : kStarPeriodMs - local; // 0..1000
        return scaled(params.color, brightness);
    });
}

KeyColorMap keyframeFrame(std::int64_t timeMs, const KeyframeTrack& track)
{
    const auto color = track.colorAt(timeMs);
    return buildFrame([&](std::size_t) { return color; });
}

KeyColorMap animationFrame(std::int64_t timeMs, const AnimationSpec& spec)
{
    return std::visit(
        [timeMs](const auto& params) {
            using Params = std::decay_t<decltype(params)>;
            if constexpr (std::is_same_v<Params, ColorCycleParams>) {
                return colorCycleFrame(timeMs, params);
            } else if constexpr (std::is_same_v<Params, WaveParams>) {
                return waveFrame(timeMs, params);
            } else if constexpr (std::is_same_v<Params, StarParams>) {
                return starFrame(timeMs, params);
            } else {
                return keyframeFrame(timeMs, params);
            }
        },
        spec);
}

KeyColorMap audioBarsFrame(const std::array<float, 10>& levels, RgbColor color)
{
    const auto total = G915KeyCatalog::keys().size();
    std::array<std::size_t, 10> bandSize{};
    for (std::size_t position = 0; position < total; ++position) {
        ++bandSize[position * 10 / total]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
    std::array<std::size_t, 10> seen{};
    return buildFrame([&](std::size_t position) {
        const auto band = position * 10 / total;
        const auto index = seen[band]++; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto raw = levels[band];   // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto level = std::isnan(raw) ? 0.0F : std::clamp(raw, 0.0F, 1.0F);
        const auto lit = static_cast<std::size_t>(
            std::ceil(static_cast<double>(level) * static_cast<double>(bandSize[band]))); // NOLINT
        return index < lit ? color : kBlack;
    });
}

} // namespace ghub::lighting::domain
