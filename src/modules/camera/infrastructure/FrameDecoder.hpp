#pragma once

#include "modules/camera/domain/PreviewMode.hpp"

#include <QImage>

#include <cstddef>
#include <span>

namespace ghub::camera {

// Converte um quadro bruto da câmera em imagem RGB do tamanho do modo. Imagem nula = quadro inválido
// (não decodifica, tamanho de dados errado ou resolução diferente do modo). Reentrante; não usa estado global.
[[nodiscard]] QImage decodeFrame(const PreviewMode& mode, std::span<const std::byte> data);

} // namespace ghub::camera
