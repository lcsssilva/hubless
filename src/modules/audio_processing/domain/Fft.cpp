#include "modules/audio_processing/domain/Fft.hpp"

#include <bit>
#include <cstddef>
#include <numbers>
#include <utility>

namespace ghub::audio_processing {

shared_kernel::Result<void> fftInPlace(std::vector<std::complex<double>>& data)
{
    const auto n = data.size();
    if (!std::has_single_bit(n)) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "o tamanho da FFT precisa ser potência de dois");
    }
    // Permutação por inversão de bits.
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1U;
        for (; (j & bit) != 0; bit >>= 1U) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(data[i], data[j]); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        }
    }
    for (std::size_t length = 2; length <= n; length <<= 1U) {
        const double angle = -2.0 * std::numbers::pi / static_cast<double>(length);
        const std::complex<double> step{std::cos(angle), std::sin(angle)};
        for (std::size_t start = 0; start < n; start += length) {
            std::complex<double> twiddle{1.0, 0.0};
            for (std::size_t k = 0; k < length / 2; ++k) {
                // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                const auto even = data[start + k];
                const auto odd = data[start + k + length / 2] * twiddle;
                data[start + k] = even + odd;
                data[start + k + length / 2] = even - odd;
                // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                twiddle *= step;
            }
        }
    }
    return {};
}

} // namespace ghub::audio_processing
