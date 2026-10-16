#pragma once

#include "modules/lighting/domain/RgbColor.hpp"
#include "modules/lighting/infrastructure/ScreenCastPortal.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace ghub::lighting::infrastructure {

// Fluxo de vídeo aberto (RAII): enquanto existir, entrega médias por região; destruir fecha o fluxo.
class ScreenStream {
public:
    virtual ~ScreenStream() = default;

protected:
    ScreenStream() = default;
    ScreenStream(const ScreenStream&) = default;
    ScreenStream& operator=(const ScreenStream&) = default;
    ScreenStream(ScreenStream&&) = default;
    ScreenStream& operator=(ScreenStream&&) = default;
};

// Chamado na thread do fluxo com as médias do último quadro; vetor vazio = o fluxo acabou (ex.: o usuário parou o
// compartilhamento) e as cores devem ser esquecidas. Nunca recebe pixels.
using RegionsCallback = std::function<void(std::vector<domain::RgbColor>)>;

// Abre o fluxo do nó `nodeId` na conexão do descritor (que passa a ser do fluxo).
using ScreenStreamFactory =
    std::function<shared_kernel::Result<std::unique_ptr<ScreenStream>>(ScreenFd, std::uint32_t, RegionsCallback)>;

} // namespace ghub::lighting::infrastructure
