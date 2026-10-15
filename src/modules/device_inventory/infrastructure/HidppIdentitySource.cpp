#include "modules/device_inventory/infrastructure/HidppIdentitySource.hpp"

#include "modules/device_inventory/infrastructure/InventoryIdentityPolicy.hpp"
#include "platform/hidpp/HidppChannel.hpp"
#include "platform/hidpp/HidrawDevice.hpp"

#include <chrono>
#include <algorithm>
#include <array>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ghub::device_inventory {

namespace {

using platform::hidpp::HidppChannel;
using platform::hidpp::HidppError;
using platform::hidpp::HidppErrorKind;

constexpr std::uint16_t featureSet = 0x0001;
constexpr std::uint16_t featureDeviceInfo = 0x0003;
constexpr std::uint16_t featureDeviceName = 0x0005;
constexpr std::size_t nameBlockSize = 16;

// Dois bytes em ordem de rede (MSB primeiro), como o HID++ envia build e ID de feature.
std::uint16_t bigEndian(std::uint8_t high, std::uint8_t low)
{
    return static_cast<std::uint16_t>((static_cast<unsigned>(high) << 8U) | low);
}

std::string hexCode(std::uint8_t code)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    return std::string{"0x"} + digits.at(code >> 4U) + digits.at(code & 0x0FU);
}

shared_kernel::DomainError toDomainError(const HidppError& error, IdentityTarget target)
{
    using shared_kernel::ErrorCode;
    switch (error.kind) {
    case HidppErrorKind::PolicyDenied:
    case HidppErrorKind::InvalidArgument:
        return {.code = ErrorCode::InvalidArgument, .detail = "pedido HID++ recusado pela política de leitura"};
    case HidppErrorKind::FeatureNotPresent:
        return {.code = ErrorCode::MalformedData, .detail = "feature HID++ obrigatória ausente no dispositivo"};
    case HidppErrorKind::Timeout:
        return {.code = ErrorCode::BackendUnavailable, .detail = "sem resposta do dispositivo (tempo esgotado)"};
    case HidppErrorKind::Io:
        return {.code = ErrorCode::BackendUnavailable, .detail = error.detail};
    case HidppErrorKind::DeviceErrorHidpp20:
        // Com o G733 desligado a resposta é o erro 0x05 (pista da captura; ADR-0023). Nos demais é erro comum.
        if (target == IdentityTarget::G733 && error.code == 0x05) {
            return {.code = ErrorCode::BackendUnavailable, .detail = "headset desligado ou fora de alcance"};
        }
        return {
            .code = ErrorCode::BackendUnavailable,
            .detail = "o dispositivo respondeu erro HID++ 2.0 (código " + hexCode(error.code) + ")",
        };
    case HidppErrorKind::DeviceErrorHidpp10:
        return {
            .code = ErrorCode::BackendUnavailable,
            .detail = "o dispositivo respondeu erro HID++ 1.0 (código " + hexCode(error.code) + ")",
        };
    }
    return {.code = ErrorCode::BackendUnavailable, .detail = "falha desconhecida na leitura HID++"};
}

// Sequência de leituras de uma sessão. Cada passo devolve o erro bruto do canal; read() o traduz uma única vez.
class IdentityReader {
public:
    IdentityReader(HidppChannel& channel, std::uint8_t deviceIndex) : channel_(&channel), deviceIndex_(deviceIndex) {}

    [[nodiscard]] std::expected<void, HidppError> readNameAndType(DeviceIdentityParts& parts)
    {
        const auto length = channel_->call(deviceIndex_, featureDeviceName, 0);
        if (!length.has_value()) {
            return absentIsNotAnError(length.error());
        }
        const std::size_t nameLength = length->at(0);
        if (nameLength >= 1 && nameLength <= DeviceIdentity::maxNameLength) {
            std::vector<std::uint8_t> bytes;
            while (bytes.size() < nameLength) {
                const std::array<std::uint8_t, 1> offset{static_cast<std::uint8_t>(bytes.size())};
                const auto block = channel_->call(deviceIndex_, featureDeviceName, 1, offset);
                if (!block.has_value()) {
                    return std::unexpected(block.error());
                }
                const auto wanted = std::min(nameBlockSize, nameLength - bytes.size());
                bytes.insert(bytes.end(), block->begin(),
                             std::next(block->begin(), static_cast<std::ptrdiff_t>(wanted)));
            }
            parts.name = deviceNameFromBytes(bytes);
        }
        const auto type = channel_->call(deviceIndex_, featureDeviceName, 2);
        if (!type.has_value()) {
            return std::unexpected(type.error());
        }
        parts.deviceType = type->at(0);
        return {};
    }

    [[nodiscard]] std::expected<void, HidppError> readFirmware(DeviceIdentityParts& parts)
    {
        const auto info = channel_->call(deviceIndex_, featureDeviceInfo, 0);
        if (!info.has_value()) {
            return absentIsNotAnError(info.error());
        }
        // Só o byte 0 (quantidade de entidades) é usado: os bytes 1-4 são o unit ID e ficam aqui, sem destino (R3).
        const std::size_t entityCount = info->at(0);
        parts.firmwareTruncated = entityCount > DeviceIdentity::maxFirmwareEntries;
        const auto toRead = std::min(entityCount, DeviceIdentity::maxFirmwareEntries);
        for (std::size_t entity = 0; entity < toRead; ++entity) {
            const std::array<std::uint8_t, 1> param{static_cast<std::uint8_t>(entity)};
            const auto fw = channel_->call(deviceIndex_, featureDeviceInfo, 1, param);
            if (!fw.has_value()) {
                return std::unexpected(fw.error());
            }
            const auto& r = *fw;
            parts.firmwares.emplace_back(r.at(0), std::array<std::uint8_t, 3>{r.at(1), r.at(2), r.at(3)}, r.at(4),
                                         r.at(5), bigEndian(r.at(6), r.at(7)), (r.at(8) & 0x01U) != 0);
        }
        return {};
    }

    [[nodiscard]] std::expected<void, HidppError> readFeatures(DeviceIdentityParts& parts)
    {
        const auto count = channel_->call(deviceIndex_, featureSet, 0);
        if (!count.has_value()) {
            return std::unexpected(count.error());
        }
        const std::size_t featureCount = count->at(0);
        parts.featuresTruncated = featureCount > DeviceIdentity::maxFeatures;
        const auto toRead = std::min(featureCount, DeviceIdentity::maxFeatures);
        // Índice 0 é a Root; as features do dispositivo ocupam 1..contagem.
        for (std::size_t index = 1; index <= toRead; ++index) {
            const std::array<std::uint8_t, 1> param{static_cast<std::uint8_t>(index)};
            const auto entry = channel_->call(deviceIndex_, featureSet, 1, param);
            if (!entry.has_value()) {
                return std::unexpected(entry.error());
            }
            parts.featureIds.push_back(bigEndian(entry->at(0), entry->at(1)));
        }
        return {};
    }

private:
    // Feature ausente (Root devolveu índice 0) significa "não informado", não falha (R5).
    static std::expected<void, HidppError> absentIsNotAnError(const HidppError& error)
    {
        if (error.kind == HidppErrorKind::FeatureNotPresent) {
            return {};
        }
        return std::unexpected(error);
    }

    HidppChannel* channel_; // não nulo; não possuído
    std::uint8_t deviceIndex_;
};

} // namespace

HidReportIoOpener hidrawOpener()
{
    return
        [](const std::filesystem::path& path) -> shared_kernel::Result<std::unique_ptr<platform::hidpp::HidReportIo>> {
            auto device = platform::hidpp::HidrawDevice::open(path);
            if (!device.has_value()) {
                return std::unexpected(device.error());
            }
            return std::unique_ptr<platform::hidpp::HidReportIo>{std::move(*device)};
        };
}

HidppIdentitySource::HidppIdentitySource(platform::hidpp::HidppNodeLocator locator, const shared_kernel::Clock& clock,
                                         HidReportIoOpener opener)
    : locator_(std::move(locator)), clock_(&clock), opener_(std::move(opener))
{
}

shared_kernel::Result<DeviceIdentity> HidppIdentitySource::read(IdentityTarget target)
{
    const auto node = [&] {
        switch (target) {
        case IdentityTarget::G915:
            return locator_.locateG915();
        case IdentityTarget::G703:
            return locator_.locateG703();
        case IdentityTarget::G733:
            return locator_.locateG733();
        }
        return shared_kernel::Result<platform::hidpp::HidppNode>{
            shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "dispositivo desconhecido")};
    }();
    if (!node.has_value()) {
        return std::unexpected(node.error());
    }
    auto io = opener_(node->devicePath);
    if (!io.has_value()) {
        return std::unexpected(io.error());
    }
    const InventoryIdentityPolicy policy; // antes do canal: a política precisa viver mais que ele
    // Prazo por pedido maior que o padrão do canal: G915/G703 dormindo levaram ~1,1 s para responder ao primeiro pedido
    // (teste físico de 2026-10-15). Os pedidos seguintes respondem em poucos ms.
    constexpr std::chrono::milliseconds requestTimeout{2000};
    HidppChannel channel{std::move(*io), *clock_, policy, requestTimeout};
    IdentityReader reader{channel, node->deviceIndex};
    DeviceIdentityParts parts;
    for (const auto step :
         {&IdentityReader::readNameAndType, &IdentityReader::readFirmware, &IdentityReader::readFeatures}) {
        if (const auto done = (reader.*step)(parts); !done.has_value()) {
            return std::unexpected(toDomainError(done.error(), target));
        }
    }
    return DeviceIdentity{std::move(parts)};
}

} // namespace ghub::device_inventory
