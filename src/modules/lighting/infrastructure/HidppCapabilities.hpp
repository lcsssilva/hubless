#pragma once

#include "modules/lighting/infrastructure/HidppChannel.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

namespace ghub::lighting::infrastructure {

struct HidppFeatureEntry {
    std::uint8_t index;
    std::uint16_t featureId;
    std::uint8_t type; // byte de tipo/flags devolvido por FeatureSet GetFeatureID

    friend bool operator==(const HidppFeatureEntry&, const HidppFeatureEntry&) = default;
};

// Leituras sobre o canal: só Root/FeatureSet e o GetInfo (fn0) das features de iluminação. Sem escrita de estado.
class HidppCapabilities {
public:
    static constexpr std::uint16_t featureEffects = 0x8071;
    static constexpr std::uint16_t featurePerKey = 0x8081;
    static constexpr std::uint16_t featureColorLed = 0x8070;

    explicit HidppCapabilities(HidppChannel& channel) : channel_(channel) {}

    // FeatureSet fn0 (contagem) + fn1 (GetFeatureID por índice 1..contagem). Ausência de FeatureSet é erro.
    [[nodiscard]] std::expected<std::vector<HidppFeatureEntry>, HidppError> enumerateFeatures(std::uint8_t deviceIndex);

    // GetInfo (fn0) bruto de 0x8070/0x8071/0x8081; nullopt se a feature estiver ausente. Formato não confirmado.
    [[nodiscard]] std::expected<std::optional<HidppChannel::Response>, HidppError> rawInfo(std::uint8_t deviceIndex,
                                                                                           std::uint16_t featureId);

private:
    HidppChannel& channel_;
};

} // namespace ghub::lighting::infrastructure
