#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace ghub::camera {

// Costura de infraestrutura sobre as chamadas de sistema do V4L2: o backend real usa LinuxV4l2Api;
// os testes usam uma API falsa que simula um driver UVC. Erros voltam como -errno.
class V4l2Api {
public:
    virtual ~V4l2Api() = default;

    // Descritor (>= 0) ou -errno.
    [[nodiscard]] virtual int open(const std::filesystem::path& node) = 0;
    // 0 em sucesso ou -errno.
    [[nodiscard]] virtual int ioctl(int fd, unsigned long request, void* argument) = 0;
    virtual void close(int fd) noexcept = 0;

    // Abre o nó para streaming: leitura e escrita (o mmap dos buffers exige), sem bloquear, close-on-exec.
    // Descritor (>= 0) ou -errno.
    [[nodiscard]] virtual int openForStreaming(const std::filesystem::path& node) = 0;
    // Mapeia `length` bytes do buffer de captura em `offset`, só para leitura e compartilhado.
    // 0 em sucesso (e `address` preenchido) ou -errno.
    [[nodiscard]] virtual int mapBuffer(int fd, std::size_t length, std::uint32_t offset,
                                        const std::byte*& address) = 0;
    virtual void unmapBuffer(const std::byte* address, std::size_t length) noexcept = 0;
    // Espera até `timeout` por eventos no descritor. Devolve os `revents` do poll (0 = prazo esgotado) ou -errno.
    [[nodiscard]] virtual int poll(int fd, std::chrono::milliseconds timeout) = 0;

protected:
    V4l2Api() = default;
    V4l2Api(const V4l2Api&) = default;
    V4l2Api& operator=(const V4l2Api&) = default;
    V4l2Api(V4l2Api&&) = default;
    V4l2Api& operator=(V4l2Api&&) = default;
};

} // namespace ghub::camera
