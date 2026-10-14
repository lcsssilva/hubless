#pragma once

#include "modules/lighting/infrastructure/HidReportIo.hpp"
#include "shared_kernel/Result.hpp"

#include <filesystem>
#include <memory>

namespace ghub::lighting::infrastructure {

// Adaptador RAII sobre um nó hidraw: write, poll + read com prazo (nunca leitura bloqueante). Fecha no destrutor.
class HidrawDevice final : public HidReportIo {
public:
    // open(path, O_RDWR | O_CLOEXEC).
    [[nodiscard]] static shared_kernel::Result<std::unique_ptr<HidrawDevice>> open(const std::filesystem::path& path);

    // Assume a posse de um descritor já aberto (usado também por testes com socketpair).
    explicit HidrawDevice(int fileDescriptor) noexcept : fd_(fileDescriptor) {}
    ~HidrawDevice() override;
    HidrawDevice(const HidrawDevice&) = delete;
    HidrawDevice& operator=(const HidrawDevice&) = delete;
    HidrawDevice(HidrawDevice&&) = delete;
    HidrawDevice& operator=(HidrawDevice&&) = delete;

    [[nodiscard]] std::expected<std::size_t, HidIoError> write(std::span<const std::uint8_t> report) override;
    [[nodiscard]] std::expected<std::size_t, HidIoError> read(std::span<std::uint8_t> buffer,
                                                              std::chrono::milliseconds timeout) override;

private:
    int fd_;
};

} // namespace ghub::lighting::infrastructure
