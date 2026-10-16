#include "modules/headset/infrastructure/G733StatusPolicy.hpp"

namespace ghub::headset::infrastructure {

namespace {

constexpr std::uint16_t rootFeature = 0x0000;
constexpr std::uint16_t equalizerFeature = 0x8310;
constexpr std::uint16_t uninterpretedFeature = 0x8300;
constexpr std::uint16_t lightingFeature = 0x8070;
constexpr std::size_t featureIdQuerySize = 2;
constexpr std::uint8_t maxSidetone = 100;
constexpr std::uint8_t lastZone = 1;
constexpr unsigned minPeriod = 1000;
constexpr unsigned maxPeriod = 20000;
constexpr unsigned periodStep = 100;
constexpr std::uint8_t maxBrightness = 100;

bool periodValid(std::uint8_t high, std::uint8_t low)
{
    const unsigned period = (static_cast<unsigned>(high) << 8U) | low;
    return period >= minPeriod && period <= maxPeriod && period % periodStep == 0;
}

bool brightnessValid(std::uint8_t value)
{
    return value >= 1 && value <= maxBrightness;
}

// 0x8070 fn3: <zona> <efeito> <parâmetros>, só nos quatro formatos das capturas C-01/C-03 (ADR-0026).
bool lightingAllowed(std::span<const std::uint8_t> p)
{
    if (p.size() < 2 || p[0] > lastZone) {
        return false;
    }
    switch (p[1]) {
    case 0: // desligado
        return p.size() == 2;
    case 1: // fixo: R G B 02
        return p.size() == 6 && p[5] == 0x02;
    case 2: // respiração: R G B <período BE16> 00 <brilho>
        return p.size() == 9 && periodValid(p[5], p[6]) && p[7] == 0x00 && brightnessValid(p[8]);
    case 3: // ciclo: 00 00 00 00 00 <período BE16> <brilho>
        return p.size() == 10 && p[2] == 0 && p[3] == 0 && p[4] == 0 && p[5] == 0 && p[6] == 0 &&
               periodValid(p[7], p[8]) && brightnessValid(p[9]);
    default:
        return false;
    }
}

} // namespace

bool G733StatusPolicy::isAllowed(std::uint16_t featureId, std::uint8_t function,
                                 std::span<const std::uint8_t> params) const noexcept
{
    switch (featureId) {
    case rootFeature:
        return function == 0 && params.size() == featureIdQuerySize;
    case equalizerFeature:
        return function == 0 && params.empty();
    case uninterpretedFeature:
        if (function == 1) { // sidetone
            return params.size() == 1 && params[0] <= maxSidetone;
        }
        return function == 0 && params.empty();
    case lightingFeature:
        return function == 3 && lightingAllowed(params);
    default:
        return false;
    }
}

} // namespace ghub::headset::infrastructure
