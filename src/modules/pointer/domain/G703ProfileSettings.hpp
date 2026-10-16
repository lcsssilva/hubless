#pragma once

#include "modules/pointer/domain/LedSetting.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace ghub::pointer {

// Parte editável de um perfil onboard do G703: taxa de relatório, estágios de DPI (com o padrão e o estágio do DPI
// shift) e os LEDs primário e logotipo. Trabalha sobre os bytes do setor (ADR-0027: o domínio não inclui a API de
// outro módulo); o CRC é responsabilidade do módulo onboard_memory.
// Formato do setor: byte 0 = taxa em ms; byte 1 = índice do DPI padrão; byte 2 = índice do estágio do DPI shift;
// bytes 3..12 = 5 DPI little-endian (o primeiro 0 encerra a lista); offset 0xD0 = entradas de LED de 11 bytes
// (0 = primário, 1 = logotipo; as entradas 2 e 3 são cópias com brilho menor e não são editadas).
// "Sem DPI shift" é gravado como 0xFF no byte 2 — convenção ainda não confirmada em hardware.
// Objeto de valor imutável; só existe válido.
class G703ProfileSettings {
public:
    // Lê do setor (taxa em {1,2,4,8} ms, ao menos 1 estágio, padrão existente). Um byte de shift fora dos estágios é
    // lido como "sem shift".
    [[nodiscard]] static shared_kernel::Result<G703ProfileSettings> read(std::span<const std::uint8_t> sector);

    // Cria a partir de valores de edição: taxa em {125,250,500,1000} Hz; 1..5 estágios de 100..25600 DPI em
    // múltiplos de 50; estágio padrão e estágio do shift (se houver) existentes.
    [[nodiscard]] static shared_kernel::Result<G703ProfileSettings>
    make(std::uint32_t reportRateHz, std::vector<std::uint16_t> dpiStages, std::uint32_t defaultStage,
         std::optional<std::uint32_t> dpiShiftStage, LedSetting primaryLed, LedSetting logoLed);

    [[nodiscard]] std::uint32_t reportRateHz() const noexcept { return reportRateHz_; }
    [[nodiscard]] const std::vector<std::uint16_t>& dpiStages() const noexcept { return dpiStages_; }
    [[nodiscard]] std::uint32_t defaultStage() const noexcept { return defaultStage_; }
    [[nodiscard]] std::optional<std::uint32_t> dpiShiftStage() const noexcept { return dpiShiftStage_; }
    [[nodiscard]] const LedSetting& primaryLed() const noexcept { return primaryLed_; }
    [[nodiscard]] const LedSetting& logoLed() const noexcept { return logoLed_; }

    // Devolve uma cópia do setor com taxa, padrão, shift, DPIs (não usados = 0) e as duas entradas de LED
    // gravados. Todos os demais bytes (inclusive o CRC antigo, que o chamador recalcula) ficam como estavam; um LED
    // igual ao já gravado e um shift que já vale não tocam seus bytes.
    [[nodiscard]] shared_kernel::Result<std::vector<std::uint8_t>>
    applyTo(std::span<const std::uint8_t> original) const;

    friend bool operator==(const G703ProfileSettings&, const G703ProfileSettings&) = default;

private:
    G703ProfileSettings(std::uint32_t reportRateHz, std::vector<std::uint16_t> dpiStages, std::uint32_t defaultStage,
                        std::optional<std::uint32_t> dpiShiftStage, LedSetting primaryLed, LedSetting logoLed)
        : reportRateHz_(reportRateHz), dpiStages_(std::move(dpiStages)), defaultStage_(defaultStage),
          dpiShiftStage_(dpiShiftStage), primaryLed_(primaryLed), logoLed_(logoLed)
    {
    }

    std::uint32_t reportRateHz_;
    std::vector<std::uint16_t> dpiStages_;
    std::uint32_t defaultStage_;
    std::optional<std::uint32_t> dpiShiftStage_;
    LedSetting primaryLed_;
    LedSetting logoLed_;
};

} // namespace ghub::pointer
