#pragma once

#include "modules/lighting/domain/G915KeyCatalog.hpp"
#include "modules/lighting/domain/KeyboardEffect.hpp"
#include "shared_kernel/Result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ghub::lighting::domain {

// Atribuição de uma tecla G do perfil onboard do G915 (INC-022 R1/R4): 4 bytes `80 02 MM KK` no setor, com MM o bitmap
// de modificadores HID (01 Ctrl, 02 Shift, 04 Alt, 08 Super) e KK o código de uso HID da tecla (índice da zona 0x01 do
// G915KeyCatalog). Ctrl+C = `80 02 01 06` (captura C-06, 2026-10-15). Qualquer outro formato é "bruto": preservado,
// nunca criado por este código. "Original" volta ao valor da cópia de segurança. Objeto de valor imutável.
class GKeyBinding {
public:
    enum class Kind : std::uint8_t { Key, Original, Raw };

    static constexpr std::uint8_t kCtrl = 0x01;
    static constexpr std::uint8_t kShift = 0x02;
    static constexpr std::uint8_t kAlt = 0x04;
    static constexpr std::uint8_t kSuper = 0x08;

    // Modificadores fora do bitmap ou tecla fora da zona de teclado do catálogo: InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<GKeyBinding> create(std::uint8_t modifiers, KeyId key);
    [[nodiscard]] static GKeyBinding original() noexcept;
    // Bytes exatamente como estão no setor; só é aceito na gravação se o setor atual ou a cópia os tiver.
    [[nodiscard]] static GKeyBinding raw(const std::array<std::uint8_t, 4>& bytes) noexcept;
    // Interpreta os 4 bytes lidos do setor: Key se for o formato conhecido, senão Raw.
    [[nodiscard]] static GKeyBinding decode(const std::array<std::uint8_t, 4>& bytes);

    [[nodiscard]] Kind kind() const noexcept { return kind_; }
    // Só Kind::Key.
    [[nodiscard]] std::uint8_t modifiers() const noexcept { return modifiers_; }
    [[nodiscard]] std::optional<KeyId> key() const noexcept { return key_; }
    // Bytes do setor para Key e Raw; zeros para Original (depende da cópia).
    [[nodiscard]] std::array<std::uint8_t, 4> encoded() const noexcept { return bytes_; }

    friend bool operator==(const GKeyBinding&, const GKeyBinding&) = default;

private:
    GKeyBinding(Kind kind, std::uint8_t modifiers, std::optional<KeyId> key, std::array<std::uint8_t, 4> bytes)
        : kind_(kind), modifiers_(modifiers), key_(key), bytes_(bytes)
    {
    }

    Kind kind_;
    std::uint8_t modifiers_;
    std::optional<KeyId> key_;
    std::array<std::uint8_t, 4> bytes_;
};

// Parte editável do setor do perfil ativo do G915 (INC-019 R4, INC-022): efeito gravado do logo e das teclas e as teclas
// G1-G5. Todo o resto do setor passa intacto por applyTo. Layout (evidência 2026-10-14/15, setor de 255 bytes, byte 0 =
// 01): teclas G em 0x20 (5 × 4 bytes) e iluminação em 0xD0 (4 × 11 bytes: logo, teclas, e as variantes de brilho
// reduzido de cada uma). Efeito `nullopt` = formato bruto: não é editável e fica como está. Objeto de valor imutável.
class G915ProfileSettings {
public:
    static constexpr std::size_t kSectorSize = 255;
    static constexpr std::size_t kGKeyCount = 5;
    using GKeys = std::array<GKeyBinding, kGKeyCount>;

    // Setor de tamanho ou formato diferente do esperado: MalformedData.
    [[nodiscard]] static shared_kernel::Result<G915ProfileSettings> read(std::span<const std::uint8_t> sector);
    // O logo só tem Static, Cycle e Breathing; as teclas não têm "Off" (use cor fixa preta). InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<G915ProfileSettings>
    make(std::optional<KeyboardEffect> logo, std::optional<KeyboardEffect> keys, GKeys gKeys);

    [[nodiscard]] const std::optional<KeyboardEffect>& logo() const noexcept { return logo_; }
    [[nodiscard]] const std::optional<KeyboardEffect>& keys() const noexcept { return keys_; }
    [[nodiscard]] const GKeys& gKeys() const noexcept { return gKeys_; }

    // Bytes do setor com estas configurações sobre `original` (CRC não recalculado aqui). `baseline` é a cópia de
    // segurança (ou o próprio setor atual, se ainda não há cópia): origem de GKeyBinding::original() e de bytes brutos.
    // Tudo fora dos campos editados fica igual a `original`.
    [[nodiscard]] shared_kernel::Result<std::vector<std::uint8_t>>
    applyTo(std::span<const std::uint8_t> original, std::span<const std::uint8_t> baseline) const;

    friend bool operator==(const G915ProfileSettings&, const G915ProfileSettings&) = default;

private:
    G915ProfileSettings(std::optional<KeyboardEffect> logo, std::optional<KeyboardEffect> keys, GKeys gKeys)
        : logo_(logo), keys_(keys), gKeys_(gKeys)
    {
    }

    std::optional<KeyboardEffect> logo_;
    std::optional<KeyboardEffect> keys_;
    GKeys gKeys_;
};

} // namespace ghub::lighting::domain
