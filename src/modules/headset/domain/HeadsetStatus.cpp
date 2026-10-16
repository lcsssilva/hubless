#include "modules/headset/domain/HeadsetStatus.hpp"

namespace ghub::headset {

namespace {

shared_kernel::DomainError malformed(const char* what)
{
    return {.code = shared_kernel::ErrorCode::MalformedData, .detail = what};
}

} // namespace

shared_kernel::Result<EqualizerInfo> parseEqualizerInfo(std::span<const std::uint8_t> response)
{
    if (response.size() < 3) {
        return std::unexpected(malformed("resposta curta das bandas do equalizador"));
    }
    if (response[0] == 0) {
        return std::unexpected(malformed("o dispositivo informou zero bandas de equalizador"));
    }
    return EqualizerInfo{.bandCount = response[0], .unexplained = {response[1], response[2]}};
}

shared_kernel::Result<UninterpretedRead> parseUninterpretedRead(std::span<const std::uint8_t> response)
{
    if (response.size() < 4) {
        return std::unexpected(malformed("resposta curta da leitura não interpretada"));
    }
    return UninterpretedRead{.bytes = {response[0], response[1], response[2], response[3]}};
}

} // namespace ghub::headset
