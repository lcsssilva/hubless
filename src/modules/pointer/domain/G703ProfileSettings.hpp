#pragma once

#include "modules/pointer/domain/OnboardSector.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace ghub::pointer {

// Parte editável de um perfil onboard do G703: taxa de relatório e estágios de DPI.
// Formato do setor (libratbag hidpp20.c): byte 0 = taxa em ms; byte 1 = índice do DPI padrão;
// byte 2 = índice do DPI shift; bytes 3..12 = 5 DPI little-endian (o primeiro 0 encerra a lista).
// Objeto de valor imutável; só existe válido.
class G703ProfileSettings {
public:
    // Lê do setor (CRC válido, taxa em {1,2,4,8} ms, ao menos 1 estágio).
    [[nodiscard]] static shared_kernel::Result<G703ProfileSettings> read(const OnboardSector& sector);

    // Cria a partir de valores de edição: taxa em {125,250,500,1000} Hz; 1..5 estágios de 100..25500 DPI em
    // múltiplos de 50; estágio padrão existente.
    [[nodiscard]] static shared_kernel::Result<G703ProfileSettings>
    make(std::uint32_t reportRateHz, std::vector<std::uint16_t> dpiStages, std::uint32_t defaultStage);

    [[nodiscard]] std::uint32_t reportRateHz() const noexcept { return reportRateHz_; }
    [[nodiscard]] const std::vector<std::uint16_t>& dpiStages() const noexcept { return dpiStages_; }
    [[nodiscard]] std::uint32_t defaultStage() const noexcept { return defaultStage_; }

    // Grava taxa, estágio padrão e DPIs (não usados = 0) no setor e recalcula o CRC. Todos os demais bytes,
    // inclusive o índice do DPI shift (byte 2), são preservados; se o shift apontar para um estágio que deixaria de
    // existir, devolve InvalidArgument sem alterar nada. O original precisa ter CRC válido.
    [[nodiscard]] shared_kernel::Result<OnboardSector> applyTo(const OnboardSector& original) const;

    friend bool operator==(const G703ProfileSettings&, const G703ProfileSettings&) = default;

private:
    G703ProfileSettings(std::uint32_t reportRateHz, std::vector<std::uint16_t> dpiStages, std::uint32_t defaultStage)
        : reportRateHz_(reportRateHz), dpiStages_(std::move(dpiStages)), defaultStage_(defaultStage)
    {
    }

    std::uint32_t reportRateHz_;
    std::vector<std::uint16_t> dpiStages_;
    std::uint32_t defaultStage_;
};

} // namespace ghub::pointer
