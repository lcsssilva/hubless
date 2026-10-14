#include "modules/audio_processing/domain/VoiceThreshold.hpp"

namespace ghub::audio_processing {

VoiceThreshold VoiceThreshold::standard() noexcept
{
    return VoiceThreshold{50};
}

shared_kernel::Result<VoiceThreshold> VoiceThreshold::create(int percent)
{
    if (percent < minimumPercent || percent > maximumPercent) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "sensibilidade fora de 0-99 %");
    }
    return VoiceThreshold{percent};
}

} // namespace ghub::audio_processing
