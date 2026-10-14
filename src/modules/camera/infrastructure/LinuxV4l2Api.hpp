#pragma once

#include "modules/camera/infrastructure/V4l2Api.hpp"

namespace ghub::camera {

// Chamadas de sistema reais. `open` abre somente leitura; `openForStreaming` abre leitura e escrita (exigência do
// mmap de buffers vb2); ambos sem bloquear e com close-on-exec.
// Sem lógica própria além de converter errno em -errno e repetir o ioctl em EINTR.
class LinuxV4l2Api final : public V4l2Api {
public:
    LinuxV4l2Api() = default;

    [[nodiscard]] int open(const std::filesystem::path& node) override;
    [[nodiscard]] int ioctl(int fd, unsigned long request, void* argument) override;
    void close(int fd) noexcept override;
    [[nodiscard]] int openForStreaming(const std::filesystem::path& node) override;
    [[nodiscard]] int mapBuffer(int fd, std::size_t length, std::uint32_t offset, const std::byte*& address) override;
    void unmapBuffer(const std::byte* address, std::size_t length) noexcept override;
    [[nodiscard]] int poll(int fd, std::chrono::milliseconds timeout) override;
};

} // namespace ghub::camera
