#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace ghub::lighting::infrastructure {

enum class HidIoFailure : std::uint8_t {
    TimedOut, // prazo esgotado sem relatório: erro distinto de falha de I/O
    Io,       // falha do sistema (errno em HidIoError::systemError)
};

struct HidIoError {
    HidIoFailure failure;
    int systemError = 0;

    friend bool operator==(const HidIoError&, const HidIoError&) = default;
};

// Port de entrada/saída de relatórios HID brutos (um relatório por chamada). Nunca bloqueia além do prazo.
class HidReportIo {
public:
    virtual ~HidReportIo() = default;

    // Escreve um relatório; devolve os bytes escritos.
    [[nodiscard]] virtual std::expected<std::size_t, HidIoError> write(std::span<const std::uint8_t> report) = 0;
    // Lê um relatório para `buffer`, esperando no máximo `timeout`; devolve os bytes lidos.
    [[nodiscard]] virtual std::expected<std::size_t, HidIoError> read(std::span<std::uint8_t> buffer,
                                                                      std::chrono::milliseconds timeout) = 0;

protected:
    HidReportIo() = default;
    HidReportIo(const HidReportIo&) = default;
    HidReportIo& operator=(const HidReportIo&) = default;
    HidReportIo(HidReportIo&&) = default;
    HidReportIo& operator=(HidReportIo&&) = default;
};

} // namespace ghub::lighting::infrastructure
