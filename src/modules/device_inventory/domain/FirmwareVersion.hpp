#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace ghub::device_inventory {

// Uma entidade de firmware da feature HID++ 0x0003 fn1 (INC-015 R7). Objeto de valor imutável.
// Número, revisão e build vêm em BCD: cada nibble é exibido como dígito hexadecimal, o que coincide com o decimal
// quando o BCD é válido e não falha quando não é (nibble > 9 aparece como A-F).
class FirmwareVersion {
public:
    // `kind`: 0 aplicação, 1 bootloader, 2 hardware, outro valor é preservado. `prefix`: 3 bytes ASCII.
    FirmwareVersion(std::uint8_t kind, std::array<std::uint8_t, 3> prefix, std::uint8_t number, std::uint8_t revision,
                    std::uint16_t build, bool active);

    // "MPK 09.04 b0042". Prefixo sem espaços (nem NUL) à direita; byte fora de ASCII imprimível vira '?'.
    [[nodiscard]] const std::string& versionText() const noexcept { return versionText_; }
    // "aplicação", "bootloader", "hardware" ou "outro (N)".
    [[nodiscard]] std::string kindText() const;
    [[nodiscard]] bool active() const noexcept { return active_; }
    // "MPK 09.04 b0042 — aplicação (ativo)"; o sufixo "(ativo)" só aparece na entrada ativa.
    [[nodiscard]] std::string describe() const;

    friend bool operator==(const FirmwareVersion&, const FirmwareVersion&) = default;

private:
    std::uint8_t kind_;
    bool active_;
    std::string versionText_;
};

} // namespace ghub::device_inventory
