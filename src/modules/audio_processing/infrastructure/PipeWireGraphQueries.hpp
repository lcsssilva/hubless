#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "shared_kernel/Result.hpp"

namespace ghub::audio_processing {

// Objeto global do registro do PipeWire: id e propriedades (texto).
struct GraphObject {
    std::uint32_t id;
    std::map<std::string, std::string> props;
};

// Foto dos nós e dispositivos do registro. Dados puros, sem libpipewire.
struct GraphSnapshot {
    std::vector<GraphObject> nodes;
    std::vector<GraphObject> devices;
};

// node.name do Audio/Sink cujo device.id aponta para um dispositivo 046d:0afe (G733). Ausente -> BackendUnavailable.
[[nodiscard]] shared_kernel::Result<std::string> findHeadsetSinkName(const GraphSnapshot& snapshot);

// node.name do Audio/Source cujo device.id aponta para um dispositivo 046d:0afe (G733). Ausente -> BackendUnavailable.
[[nodiscard]] shared_kernel::Result<std::string> findHeadsetSourceName(const GraphSnapshot& snapshot);

// Valor de uma chave default.configured.audio.*: {"name":"<node.name>"}.
[[nodiscard]] std::string makeDefaultNodeValue(const std::string& nodeName);
// Extrai o nome; texto mal formado ou sem "name" textual -> MalformedData citando metadataKey.
[[nodiscard]] shared_kernel::Result<std::string> parseDefaultNodeValue(const std::string& value,
                                                                       const std::string& metadataKey);

// Valor da chave default.configured.audio.sink: {"name":"<node.name>"}.
[[nodiscard]] std::string makeDefaultSinkValue(const std::string& sinkName);
// Extrai o nome; texto mal formado ou sem "name" textual -> MalformedData.
[[nodiscard]] shared_kernel::Result<std::string> parseDefaultSinkValue(const std::string& value);

} // namespace ghub::audio_processing
