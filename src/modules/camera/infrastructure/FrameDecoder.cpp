#include "modules/camera/infrastructure/FrameDecoder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace ghub::camera {

namespace {

// Limite defensivo: modos maiores que isso não são decodificados (evita alocação desproporcional).
constexpr std::uint32_t maxDimension = 4096;

// Valor em ponto fixo de 8 bits -> canal 0–255, com saturação (negativo vira 0; acima de 255 vira 255).
int toChannel(int fixed)
{
    return std::clamp(fixed, 0, (255 * 256) + 255) / 256;
}

// BT.601, faixa limitada (Y 16–235, C 16–240), aritmética inteira com ponto fixo de 8 bits.
QRgb yuvToRgb(int y, int u, int v)
{
    const int c = 298 * (y - 16);
    const int d = u - 128;
    const int e = v - 128;
    const int r = toChannel(c + (409 * e) + 128);
    const int g = toChannel(c - (100 * d) - (208 * e) + 128);
    const int b = toChannel(c + (516 * d) + 128);
    return qRgb(r, g, b);
}

QImage decodeYuyv(const PreviewMode& mode, std::span<const std::byte> data)
{
    if (mode.width == 0 || mode.height == 0 || mode.width % 2 != 0) {
        return {};
    }
    const std::uint64_t expected = std::uint64_t{mode.width} * mode.height * 2;
    if (data.size() != expected) {
        return {};
    }
    QImage image(static_cast<int>(mode.width), static_cast<int>(mode.height), QImage::Format_RGB32);
    if (image.isNull()) {
        return {};
    }
    // Acesso direto às linhas do QImage (scanLine, alinhadas a 4 bytes) e aos bytes do quadro; o tamanho
    // de `data` foi validado acima e o laço percorre exatamente width/2 pares por linha.
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    std::size_t offset = 0;
    for (int row = 0; row < image.height(); ++row) {
        auto* out = reinterpret_cast<QRgb*>(image.scanLine(row));
        for (int column = 0; column < image.width(); column += 2) {
            const int y0 = std::to_integer<int>(data[offset]);
            const int u = std::to_integer<int>(data[offset + 1]);
            const int y1 = std::to_integer<int>(data[offset + 2]);
            const int v = std::to_integer<int>(data[offset + 3]);
            offset += 4;
            out[column] = yuvToRgb(y0, u, v);
            out[column + 1] = yuvToRgb(y1, u, v);
        }
    }
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    return image;
}

QImage decodeMjpg(const PreviewMode& mode, std::span<const std::byte> data)
{
    if (data.empty() || data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    // QImage::fromData recebe uchar*; std::byte tem o mesmo layout.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* bytes = reinterpret_cast<const uchar*>(data.data());
    const QImage image = QImage::fromData(bytes, static_cast<int>(data.size()), "JPG");
    if (image.isNull() || std::cmp_not_equal(image.width(), mode.width) ||
        std::cmp_not_equal(image.height(), mode.height)) {
        return {};
    }
    return image.convertToFormat(QImage::Format_RGB32);
}

} // namespace

QImage decodeFrame(const PreviewMode& mode, std::span<const std::byte> data)
{
    if (mode.width > maxDimension || mode.height > maxDimension) {
        return {};
    }
    const auto fourcc = mode.format.fourcc();
    if (fourcc == "YUYV") {
        return decodeYuyv(mode, data);
    }
    if (fourcc == "MJPG") {
        return decodeMjpg(mode, data);
    }
    return {};
}

} // namespace ghub::camera
