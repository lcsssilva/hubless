#pragma once

#include "modules/battery/infrastructure/UniqueFd.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>

namespace ghub::battery {

// Fonte de relatórios HID++ que só LÊ. A interface não tem nenhum método de escrita: quem a usa não consegue enviar
// nada ao dispositivo (garantia estrutural, ADR-0026 adendo).
class PassiveReportSource {
public:
    PassiveReportSource() = default;
    virtual ~PassiveReportSource() = default;
    PassiveReportSource(const PassiveReportSource&) = delete;
    PassiveReportSource& operator=(const PassiveReportSource&) = delete;
    PassiveReportSource(PassiveReportSource&&) = delete;
    PassiveReportSource& operator=(PassiveReportSource&&) = delete;

    // Descritor para monitorar a legibilidade (QSocketNotifier).
    [[nodiscard]] virtual int fd() const = 0;
    // Lê um relatório sem bloquear. Valor 0 = nada disponível agora; erro = errno (EOF/desconexão vira ENODEV).
    [[nodiscard]] virtual std::expected<std::size_t, int> readReport(std::span<std::uint8_t> buffer) = 0;
};

// Descritor já aberto somente para leitura, não bloqueante.
class FdPassiveReportSource final : public PassiveReportSource {
public:
    explicit FdPassiveReportSource(UniqueFd fd) noexcept;

    [[nodiscard]] int fd() const override;
    [[nodiscard]] std::expected<std::size_t, int> readReport(std::span<std::uint8_t> buffer) override;

private:
    UniqueFd fd_;
};

// Abre `path` com O_RDONLY | O_NONBLOCK | O_CLOEXEC (nunca O_RDWR). Erro = errno.
[[nodiscard]] std::expected<std::unique_ptr<PassiveReportSource>, int>
openReadOnlyReportSource(const std::filesystem::path& path);

} // namespace ghub::battery
