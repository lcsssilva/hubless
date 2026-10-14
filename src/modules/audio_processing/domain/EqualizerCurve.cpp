#include "modules/audio_processing/domain/EqualizerCurve.hpp"

#include <algorithm>

namespace ghub::audio_processing {

namespace {

bool inRange(int gainDb) noexcept
{
    return gainDb >= EqualizerCurve::minimumGainDb && gainDb <= EqualizerCurve::maximumGainDb;
}

} // namespace

EqualizerCurve EqualizerCurve::flat() noexcept
{
    return EqualizerCurve{{}};
}

shared_kernel::Result<EqualizerCurve> EqualizerCurve::create(const std::array<int, bandCount>& gainsDb)
{
    if (!std::ranges::all_of(gainsDb, inRange)) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "ganho do EQ fora de -12..12 dB");
    }
    return EqualizerCurve{gainsDb};
}

shared_kernel::Result<EqualizerCurve> EqualizerCurve::withBand(std::size_t band, int gainDb) const
{
    if (band >= bandCount) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "banda do EQ inexistente");
    }
    auto gains = gainsDb_;
    gains.at(band) = gainDb;
    return create(gains);
}

int EqualizerCurve::preampDb() const noexcept
{
    return -std::max(0, std::ranges::max(gainsDb_));
}

} // namespace ghub::audio_processing
