#include "modules/pointer/infrastructure/G703OnboardPolicy.hpp"

#include <algorithm>
#include <array>

namespace ghub::pointer::infrastructure {

namespace {

constexpr std::uint16_t rootFeature = 0x0000;
constexpr std::uint16_t featureSetFeature = 0x0001;
constexpr std::uint16_t onboardFeature = 0x8100;
constexpr std::size_t addressWriteParams = 6;
constexpr std::uint16_t sensorDpiFeature = 0x2201;
constexpr std::uint16_t reportRateFeature = 0x8060;

} // namespace

bool G703OnboardPolicy::isAllowed(std::uint16_t featureId, std::uint8_t function,
                                  std::span<const std::uint8_t> params) const noexcept
{
    if (featureId == rootFeature || featureId == featureSetFeature) {
        return function == 0 || function == 1;
    }
    if (featureId == sensorDpiFeature) {
        // Leituras (INC-015 parte 2, ADR-0025): fn1 lista de DPI e fn2 DPI atual/padrão, só do sensor 0. A fn3
        // (SetSensorDpi) e qualquer outra ficam recusadas.
        return (function == 1 || function == 2) && params.size() == 1 && params[0] == 0x00;
    }
    if (featureId == reportRateFeature) {
        // fn0 períodos suportados e fn1 período atual, sem parâmetros. A fn2 (SetReportRate) fica recusada.
        return (function == 0 || function == 1) && params.empty();
    }
    if (featureId != onboardFeature) {
        return false;
    }
    switch (function) {
    case 0:
    case 2:
    case 4:
    case 5:
    case 7:
    case 8:
        return true;
    case 6: {
        if (params.size() < addressWriteParams) {
            return false;
        }
        std::array<std::uint8_t, 4> head{};
        std::copy_n(params.begin(), head.size(), head.begin());
        const auto sector = (static_cast<unsigned>(std::get<0>(head)) << 8U) | std::get<1>(head);
        return sector >= 1 && sector <= profileCount_ && std::get<2>(head) == 0 && std::get<3>(head) == 0;
    }
    default:
        return false;
    }
}

} // namespace ghub::pointer::infrastructure
