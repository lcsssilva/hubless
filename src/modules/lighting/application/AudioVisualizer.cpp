#include "modules/lighting/application/AudioVisualizer.hpp"

#include "modules/lighting/domain/AnimationFrames.hpp"

#include <cstdint>

namespace ghub::lighting::application {

AudioVisualizer::AudioVisualizer(audio_processing::AudioLevelSource& audio, domain::RgbColor color) noexcept
    : audio_(&audio), color_(color)
{
}

AudioVisualizer::~AudioVisualizer()
{
    stop();
}

shared_kernel::Result<void> AudioVisualizer::start()
{
    return audio_->start();
}

void AudioVisualizer::stop()
{
    audio_->stop();
}

RunSoftwareAnimation::FrameSource AudioVisualizer::frameSource() const
{
    return [audio = audio_, color = color_](std::int64_t) { return domain::audioBarsFrame(audio->levels(), color); };
}

} // namespace ghub::lighting::application
