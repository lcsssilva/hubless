#include "modules/camera/domain/PixelFormat.hpp"

#include <algorithm>

namespace ghub::camera {

shared_kernel::Result<PixelFormat> PixelFormat::create(std::string_view fourcc)
{
    constexpr auto isPrintable = [](char c) {
        const auto code = static_cast<unsigned char>(c);
        return code >= 0x21 && code <= 0x7E;
    };
    if (fourcc.size() != 4 || !std::ranges::all_of(fourcc, isPrintable)) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "FourCC precisa de 4 caracteres ASCII imprimíveis sem espaço");
    }
    std::array<char, 4> code{};
    std::ranges::copy(fourcc, code.begin());
    return PixelFormat{code};
}

} // namespace ghub::camera
