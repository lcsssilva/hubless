#pragma once

#include "modules/lighting/infrastructure/ScreenStream.hpp"

#include <string>

namespace ghub::lighting::infrastructure {

// Fluxo de vídeo (`pw_stream`) do monitor concedido pelo portal ScreenCast (INC-021 R4, ADR-0030). Pede quadros em
// memória (BGRx/RGBx/BGRA/RGBA; nada de DMA-BUF), reduz CADA quadro na hora a 10 médias (uma linha de 10 colunas) e
// descarta o buffer; nenhum quadro é guardado, copiado ou registrado em log. No máximo 20 quadros por segundo são
// reduzidos. Destruir fecha o fluxo e a conexão (e o descritor do portal).
class PipeWireScreenStream final : public ScreenStream {
public:
    // `fd`: conexão PipeWire concedida pelo portal (passa a ser do fluxo). Para testes contra um daemon privado,
    // passe um `fd` inválido e o caminho do socket em `remoteName`. `nodeId` 0 = qualquer nó. Falha (nada fica
    // aberto) -> BackendUnavailable.
    [[nodiscard]] static shared_kernel::Result<std::unique_ptr<ScreenStream>>
    open(ScreenFd fd, std::uint32_t nodeId, RegionsCallback onRegions, const std::string& remoteName = {});

    ~PipeWireScreenStream() override;
    PipeWireScreenStream(const PipeWireScreenStream&) = delete;
    PipeWireScreenStream& operator=(const PipeWireScreenStream&) = delete;
    PipeWireScreenStream(PipeWireScreenStream&&) = delete;
    PipeWireScreenStream& operator=(PipeWireScreenStream&&) = delete;

private:
    class Impl;
    explicit PipeWireScreenStream(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace ghub::lighting::infrastructure
