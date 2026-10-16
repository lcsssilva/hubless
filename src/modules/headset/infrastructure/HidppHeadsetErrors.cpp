#include "modules/headset/infrastructure/HidppHeadsetErrors.hpp"

#include <string>
#include <string_view>

namespace ghub::headset::infrastructure {

namespace {

std::string hexCode(std::uint8_t code)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    return std::string{"0x"} + digits.at(code >> 4U) + digits.at(code & 0x0FU);
}

} // namespace

shared_kernel::DomainError toDomainError(const platform::hidpp::HidppError& error)
{
    using platform::hidpp::HidppErrorKind;
    using shared_kernel::ErrorCode;
    switch (error.kind) {
    case HidppErrorKind::PolicyDenied:
    case HidppErrorKind::InvalidArgument:
        return {.code = ErrorCode::InvalidArgument, .detail = "pedido HID++ recusado pela política do headset"};
    case HidppErrorKind::FeatureNotPresent:
        return {.code = ErrorCode::MalformedData, .detail = "feature HID++ ausente no dispositivo"};
    case HidppErrorKind::Timeout:
        return {.code = ErrorCode::BackendUnavailable, .detail = "sem resposta (dispositivo dormindo ou desligado?)"};
    case HidppErrorKind::Io:
        return {.code = ErrorCode::BackendUnavailable, .detail = error.detail};
    case HidppErrorKind::DeviceErrorHidpp20:
        if (error.code == 0x05) {
            return {.code = ErrorCode::BackendUnavailable, .detail = "headset desligado ou fora de alcance"};
        }
        return {.code = ErrorCode::BackendUnavailable,
                .detail = "o dispositivo respondeu erro HID++ 2.0 (código " + hexCode(error.code) + ")"};
    case HidppErrorKind::DeviceErrorHidpp10:
        return {.code = ErrorCode::BackendUnavailable,
                .detail = "o dispositivo respondeu erro HID++ 1.0 (código " + hexCode(error.code) + ")"};
    }
    return {.code = ErrorCode::BackendUnavailable, .detail = "falha desconhecida na comunicação HID++"};
}

} // namespace ghub::headset::infrastructure
