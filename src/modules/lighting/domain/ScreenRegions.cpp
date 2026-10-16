#include "modules/lighting/domain/ScreenRegions.hpp"

#include "modules/lighting/domain/G915KeyCatalog.hpp"

#include <cstddef>
#include <utility>

namespace ghub::lighting::domain {

namespace {

constexpr std::size_t kBytesPerPixel = 4;
constexpr std::size_t kBands = 10;

shared_kernel::Result<std::vector<RgbColor>> invalid(const char* why)
{
    return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, why);
}

// Início da região `index` de `count` sobre `size` pixels; 64 bits para não estourar com telas grandes.
std::uint32_t boundary(std::uint32_t index, std::uint32_t count, std::uint32_t size)
{
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(index) * size / count);
}

} // namespace

shared_kernel::Result<std::vector<RgbColor>> averageRegions(const ScreenFrameView& frame, RegionGrid grid,
                                                            std::uint32_t sampleStep)
{
    if (frame.width == 0 || frame.height == 0) {
        return invalid("quadro sem dimensões");
    }
    if (grid.columns == 0 || grid.rows == 0 || grid.columns > frame.width || grid.rows > frame.height) {
        return invalid("grade inválida para o quadro");
    }
    const std::size_t rowBytes = static_cast<std::size_t>(frame.width) * kBytesPerPixel;
    if (frame.stride < rowBytes) {
        return invalid("stride menor que a linha");
    }
    if (frame.pixels.size() < (static_cast<std::size_t>(frame.height - 1) * frame.stride) + rowBytes) {
        return invalid("buffer menor que o quadro");
    }
    const auto step = sampleStep == 0 ? 1U : sampleStep;
    const std::size_t redOffset = frame.format == PixelFormat::Bgrx ? 2 : 0;
    const std::size_t blueOffset = frame.format == PixelFormat::Bgrx ? 0 : 2;

    std::vector<RgbColor> regions;
    regions.reserve(static_cast<std::size_t>(grid.columns) * grid.rows);
    for (std::uint32_t row = 0; row < grid.rows; ++row) {
        const auto y0 = boundary(row, grid.rows, frame.height);
        const auto y1 = boundary(row + 1, grid.rows, frame.height);
        for (std::uint32_t column = 0; column < grid.columns; ++column) {
            const auto x0 = boundary(column, grid.columns, frame.width);
            const auto x1 = boundary(column + 1, grid.columns, frame.width);
            std::uint64_t red = 0;
            std::uint64_t green = 0;
            std::uint64_t blue = 0;
            std::uint64_t count = 0;
            for (auto y = y0; y < y1; y += step) {
                const auto line = frame.pixels.subspan(static_cast<std::size_t>(y) * frame.stride, rowBytes);
                for (auto x = x0; x < x1; x += step) {
                    const auto pixel = line.subspan(static_cast<std::size_t>(x) * kBytesPerPixel, kBytesPerPixel);
                    red += pixel[redOffset];
                    green += pixel[1];
                    blue += pixel[blueOffset];
                    ++count;
                }
            }
            const auto average = [count](std::uint64_t sum) {
                return static_cast<std::uint8_t>((sum + (count / 2)) / count);
            };
            regions.emplace_back(average(red), average(green), average(blue)); // região tem ao menos um pixel
        }
    }
    return regions;
}

KeyColorMap screenRegionsFrame(std::span<const RgbColor> regions)
{
    const auto total = G915KeyCatalog::keys().size();
    std::vector<std::pair<KeyId, RgbColor>> entries;
    entries.reserve(total);
    for (const auto& key : G915KeyCatalog::keys()) {
        const auto band = key.id.position() * kBands / total;
        entries.emplace_back(key.id, regions.size() == kBands ? regions[band] : RgbColor{0, 0, 0});
    }
    auto map = KeyColorMap::create(entries);
    return map.has_value() ? *std::move(map) : KeyColorMap{}; // chaves do catálogo são únicas: nunca falha
}

} // namespace ghub::lighting::domain
