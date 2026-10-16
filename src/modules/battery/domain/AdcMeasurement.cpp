#include "modules/battery/domain/AdcMeasurement.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace ghub::battery {

namespace {

// Curva de descarga do G HUB para o G733 (ADR-0033): tensão em volts e energia acumulada em mWh, na ordem do arquivo
// original (tensão não crescente; há tensões repetidas).
struct CurvePoint {
    float volts;
    float energyMwh;
};

constexpr std::array<CurvePoint, 226> curve{{
    {3.871f, 14.25f},   {3.868f, 28.46f},   {3.865f, 42.56f},   {3.862f, 56.58f},   {3.860f, 70.6f},
    {3.859f, 84.65f},   {3.856f, 98.73f},   {3.855f, 112.78f},  {3.853f, 126.84f},  {3.852f, 140.9f},
    {3.850f, 154.94f},  {3.849f, 168.96f},  {3.847f, 182.98f},  {3.846f, 197.0f},   {3.845f, 211.07f},
    {3.843f, 225.1f},   {3.842f, 239.11f},  {3.841f, 253.11f},  {3.840f, 267.09f},  {3.839f, 281.06f},
    {3.838f, 295.02f},  {3.836f, 309.07f},  {3.835f, 323.13f},  {3.834f, 337.1f},   {3.833f, 351.05f},
    {3.832f, 365.0f},   {3.831f, 378.93f},  {3.830f, 392.87f},  {3.829f, 406.79f},  {3.828f, 420.72f},
    {3.827f, 434.64f},  {3.826f, 448.55f},  {3.825f, 462.46f},  {3.824f, 476.37f},  {3.823f, 490.28f},
    {3.822f, 504.2f},   {3.821f, 518.12f},  {3.820f, 532.09f},  {3.819f, 546.05f},  {3.818f, 560.09f},
    {3.817f, 574.25f},  {3.816f, 588.52f},  {3.816f, 602.82f},  {3.815f, 617.11f},  {3.814f, 631.41f},
    {3.813f, 645.67f},  {3.812f, 659.89f},  {3.811f, 674.04f},  {3.810f, 688.11f},  {3.810f, 702.13f},
    {3.809f, 716.19f},  {3.808f, 730.25f},  {3.807f, 744.21f},  {3.807f, 758.21f},  {3.806f, 772.21f},
    {3.805f, 786.16f},  {3.804f, 800.08f},  {3.803f, 814.04f},  {3.803f, 828.03f},  {3.802f, 842.0f},
    {3.801f, 855.99f},  {3.800f, 869.99f},  {3.800f, 883.95f},  {3.799f, 897.92f},  {3.798f, 911.86f},
    {3.798f, 925.89f},  {3.797f, 939.86f},  {3.796f, 953.86f},  {3.796f, 967.82f},  {3.795f, 981.79f},
    {3.794f, 995.77f},  {3.794f, 1009.75f}, {3.793f, 1023.73f}, {3.792f, 1037.75f}, {3.792f, 1051.73f},
    {3.791f, 1065.71f}, {3.790f, 1079.69f}, {3.790f, 1093.68f}, {3.789f, 1107.65f}, {3.789f, 1121.63f},
    {3.788f, 1135.76f}, {3.788f, 1149.74f}, {3.787f, 1163.74f}, {3.786f, 1177.71f}, {3.786f, 1191.68f},
    {3.785f, 1205.65f}, {3.784f, 1219.62f}, {3.784f, 1233.6f},  {3.783f, 1247.55f}, {3.783f, 1261.52f},
    {3.782f, 1275.46f}, {3.782f, 1289.41f}, {3.781f, 1303.35f}, {3.781f, 1317.29f}, {3.780f, 1331.23f},
    {3.779f, 1345.18f}, {3.779f, 1359.14f}, {3.778f, 1373.08f}, {3.778f, 1387.04f}, {3.777f, 1400.97f},
    {3.776f, 1414.92f}, {3.776f, 1428.85f}, {3.775f, 1442.8f},  {3.775f, 1456.75f}, {3.774f, 1470.7f},
    {3.773f, 1484.65f}, {3.772f, 1498.59f}, {3.771f, 1512.5f},  {3.771f, 1526.44f}, {3.769f, 1540.55f},
    {3.769f, 1554.52f}, {3.767f, 1568.47f}, {3.766f, 1582.42f}, {3.765f, 1596.34f}, {3.763f, 1610.29f},
    {3.762f, 1624.21f}, {3.761f, 1638.13f}, {3.760f, 1652.05f}, {3.759f, 1665.98f}, {3.758f, 1679.9f},
    {3.757f, 1693.81f}, {3.756f, 1707.73f}, {3.755f, 1721.65f}, {3.754f, 1735.55f}, {3.754f, 1749.46f},
    {3.753f, 1763.35f}, {3.752f, 1777.27f}, {3.751f, 1791.17f}, {3.750f, 1805.11f}, {3.749f, 1819.0f},
    {3.748f, 1832.91f}, {3.747f, 1846.81f}, {3.746f, 1860.7f},  {3.745f, 1874.62f}, {3.744f, 1888.51f},
    {3.743f, 1902.41f}, {3.742f, 1916.31f}, {3.741f, 1930.21f}, {3.741f, 1944.17f}, {3.740f, 1958.15f},
    {3.739f, 1972.04f}, {3.739f, 1985.93f}, {3.738f, 1999.8f},  {3.737f, 2013.66f}, {3.736f, 2027.52f},
    {3.736f, 2041.4f},  {3.735f, 2055.27f}, {3.734f, 2069.14f}, {3.733f, 2083.0f},  {3.732f, 2096.86f},
    {3.731f, 2110.73f}, {3.730f, 2124.56f}, {3.729f, 2138.42f}, {3.728f, 2152.28f}, {3.727f, 2166.15f},
    {3.725f, 2180.03f}, {3.724f, 2193.9f},  {3.722f, 2207.77f}, {3.721f, 2221.67f}, {3.719f, 2235.54f},
    {3.718f, 2249.39f}, {3.716f, 2263.28f}, {3.715f, 2277.14f}, {3.714f, 2290.99f}, {3.712f, 2304.84f},
    {3.711f, 2318.7f},  {3.710f, 2332.55f}, {3.708f, 2346.45f}, {3.707f, 2360.55f}, {3.705f, 2374.5f},
    {3.704f, 2388.47f}, {3.702f, 2402.43f}, {3.700f, 2416.39f}, {3.698f, 2430.32f}, {3.696f, 2444.24f},
    {3.694f, 2458.16f}, {3.692f, 2472.07f}, {3.690f, 2485.97f}, {3.688f, 2499.84f}, {3.687f, 2513.73f},
    {3.686f, 2527.64f}, {3.686f, 2541.55f}, {3.685f, 2555.44f}, {3.685f, 2569.33f}, {3.685f, 2583.24f},
    {3.684f, 2597.11f}, {3.684f, 2610.96f}, {3.684f, 2624.81f}, {3.683f, 2638.64f}, {3.683f, 2652.52f},
    {3.683f, 2666.37f}, {3.683f, 2680.21f}, {3.682f, 2694.04f}, {3.682f, 2707.87f}, {3.682f, 2721.71f},
    {3.681f, 2735.53f}, {3.681f, 2749.49f}, {3.680f, 2763.35f}, {3.679f, 2777.19f}, {3.679f, 2791.03f},
    {3.678f, 2804.89f}, {3.677f, 2818.71f}, {3.676f, 2832.55f}, {3.675f, 2846.36f}, {3.673f, 2860.25f},
    {3.671f, 2874.06f}, {3.668f, 2887.86f}, {3.664f, 2901.65f}, {3.658f, 2915.45f}, {3.650f, 2929.24f},
    {3.642f, 2943.01f}, {3.632f, 2956.77f}, {3.623f, 2970.57f}, {3.613f, 2984.31f}, {3.602f, 2998.04f},
    {3.591f, 3011.74f}, {3.578f, 3025.43f}, {3.566f, 3039.13f}, {3.552f, 3052.79f}, {3.537f, 3066.42f},
    {3.520f, 3080.25f}, {3.502f, 3093.9f},  {3.483f, 3107.56f}, {3.462f, 3121.19f}, {3.460f, 3122.32f},
    {3.460f, 3122.37f},
}};

constexpr std::size_t responseSize = 3;
constexpr std::uint8_t flagDischarging = 0x01;
constexpr std::uint8_t flagCharging = 0x03;
constexpr std::uint8_t flagFull = 0x07;

ChargeState stateFromFlags(std::uint8_t flags) noexcept
{
    switch (flags) {
    case flagDischarging:
        return ChargeState::Discharging;
    case flagCharging:
        return ChargeState::Charging;
    case flagFull:
        return ChargeState::Full;
    default:
        return ChargeState::Unknown;
    }
}

} // namespace

int capacityFromMillivolts(int millivolts) noexcept
{
    const float highest = curve.front().volts;
    const float lowest = curve.back().volts;
    // Primeira linha (na ordem do arquivo) da menor tensão: energia total da curva.
    const auto lowestRow = std::ranges::find_if(curve, [lowest](const CurvePoint& p) { return p.volts == lowest; });
    const float totalEnergy = lowestRow->energyMwh;

    // Fora da faixa da curva, usa a linha da tensão extrema mais próxima.
    const float volts = std::clamp(static_cast<float>(millivolts) * 0.001F, lowest, highest);
    // Menor tensão >= volts; entre repetidas, a primeira linha (lower_bound estável).
    float chosenVolts = highest;
    for (const CurvePoint& point : curve) {
        if (point.volts >= volts) {
            chosenVolts = point.volts;
        }
    }
    const auto row = std::ranges::find_if(curve, [chosenVolts](const CurvePoint& p) { return p.volts == chosenVolts; });
    const float percent = std::roundf(((totalEnergy - row->energyMwh) / totalEnergy) * 100.0F);
    return static_cast<int>(percent);
}

shared_kernel::Result<BatteryReading> readingFromAdc(std::span<const std::uint8_t> response)
{
    if (response.size() < responseSize) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData, "resposta 0x1f20 curta");
    }
    const int millivolts = (response[0] << 8U) | response[1];
    const auto level = BatteryLevel::fromPercent(capacityFromMillivolts(millivolts));
    if (!level.has_value()) {
        return std::unexpected(level.error());
    }
    return BatteryReading{stateFromFlags(response[2]), *level};
}

} // namespace ghub::battery
