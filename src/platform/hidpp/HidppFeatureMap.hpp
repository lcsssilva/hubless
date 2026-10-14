#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <utility>

namespace ghub::platform::hidpp {

// Cache de sessão: (índice de dispositivo, feature ID) -> índice de feature, preenchido somente a partir de
// respostas de Root GetFeature. Índice 0 para uma feature diferente de Root significa "ausente" (guardado também,
// para não repetir a consulta).
class HidppFeatureMap {
public:
    // nullopt = ainda não consultado; 0 = ausente; outro valor = índice resolvido.
    [[nodiscard]] std::optional<std::uint8_t> find(std::uint8_t deviceIndex, std::uint16_t featureId) const;
    void record(std::uint8_t deviceIndex, std::uint16_t featureId, std::uint8_t featureIndex);

private:
    std::map<std::pair<std::uint8_t, std::uint16_t>, std::uint8_t> entries_;
};

} // namespace ghub::platform::hidpp
