#include "modules/lighting/domain/G915ProfileSettings.hpp"

#include "modules/lighting/domain/StoredEffect.hpp"

#include <algorithm>
#include <string>

namespace ghub::lighting::domain {

namespace {

using shared_kernel::ErrorCode;

constexpr std::uint8_t kProfileFormat = 0x01;
constexpr std::size_t kGKeysOffset = 0x20;
constexpr std::size_t kGKeySize = 4;
constexpr std::size_t kLightingOffset = 0xD0;
constexpr std::uint8_t kKeyboardZone = 0x01;
constexpr std::uint8_t kGKeyTypeByte = 0x80;
constexpr std::uint8_t kGKeyKeyboardByte = 0x02;
constexpr std::uint8_t kModifierMask = 0x0F;

// Posição das entradas: 0 = logo, 1 = teclas, 2/3 = variantes de brilho reduzido de cada uma.
constexpr std::size_t kLogoEntry = 0;
constexpr std::size_t kKeysEntry = 1;
constexpr std::size_t kLogoVariant = 2;
constexpr std::size_t kKeysVariant = 3;

shared_kernel::Result<void> checkSector(std::span<const std::uint8_t> sector)
{
    if (sector.size() != G915ProfileSettings::kSectorSize) {
        return shared_kernel::fail(ErrorCode::MalformedData,
                                   "setor do perfil do G915 com tamanho inesperado: " + std::to_string(sector.size()));
    }
    if (sector.front() != kProfileFormat) {
        return shared_kernel::fail(ErrorCode::MalformedData, "formato de perfil do G915 não reconhecido");
    }
    return {};
}

StoredEffectBytes entryOf(std::span<const std::uint8_t> sector, std::size_t index)
{
    StoredEffectBytes entry{};
    std::copy_n(sector.begin() + static_cast<std::ptrdiff_t>(kLightingOffset + index * kStoredEffectSize),
                kStoredEffectSize, entry.begin());
    return entry;
}

void putEntry(std::vector<std::uint8_t>& sector, std::size_t index, const StoredEffectBytes& entry)
{
    std::ranges::copy(entry,
                      sector.begin() + static_cast<std::ptrdiff_t>(kLightingOffset + index * kStoredEffectSize));
}

std::array<std::uint8_t, 4> gKeyOf(std::span<const std::uint8_t> sector, std::size_t index)
{
    std::array<std::uint8_t, 4> bytes{};
    std::copy_n(sector.begin() + static_cast<std::ptrdiff_t>(kGKeysOffset + index * kGKeySize), kGKeySize,
                bytes.begin());
    return bytes;
}

} // namespace

shared_kernel::Result<GKeyBinding> GKeyBinding::create(std::uint8_t modifiers, KeyId key)
{
    if ((modifiers & ~kModifierMask) != 0) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "modificadores fora do bitmap Ctrl/Shift/Alt/Super");
    }
    const auto& definition = G915KeyCatalog::definition(key);
    if (definition.zone != kKeyboardZone) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "a tecla não é uma tecla de teclado atribuível");
    }
    return GKeyBinding{Kind::Key, modifiers, key, {kGKeyTypeByte, kGKeyKeyboardByte, modifiers, definition.index}};
}

GKeyBinding GKeyBinding::original() noexcept
{
    return GKeyBinding{Kind::Original, 0, std::nullopt, {}};
}

GKeyBinding GKeyBinding::raw(const std::array<std::uint8_t, 4>& bytes) noexcept
{
    return GKeyBinding{Kind::Raw, 0, std::nullopt, bytes};
}

GKeyBinding GKeyBinding::decode(const std::array<std::uint8_t, 4>& bytes)
{
    if (bytes.at(0) == kGKeyTypeByte && bytes.at(1) == kGKeyKeyboardByte && (bytes.at(2) & ~kModifierMask) == 0) {
        if (const auto key = G915KeyCatalog::findByAddress(kKeyboardZone, bytes.at(3)); key.has_value()) {
            return GKeyBinding{Kind::Key, bytes.at(2), key, bytes};
        }
    }
    return raw(bytes);
}

shared_kernel::Result<G915ProfileSettings> G915ProfileSettings::read(std::span<const std::uint8_t> sector)
{
    if (const auto ok = checkSector(sector); !ok.has_value()) {
        return std::unexpected(ok.error());
    }
    GKeys gKeys{GKeyBinding::original(), GKeyBinding::original(), GKeyBinding::original(), GKeyBinding::original(),
                GKeyBinding::original()};
    for (std::size_t i = 0; i < kGKeyCount; ++i) {
        gKeys.at(i) = GKeyBinding::decode(gKeyOf(sector, i));
    }
    return G915ProfileSettings{decodeStoredEffect(entryOf(sector, kLogoEntry)),
                               decodeStoredEffect(entryOf(sector, kKeysEntry)), gKeys};
}

shared_kernel::Result<G915ProfileSettings> G915ProfileSettings::make(std::optional<KeyboardEffect> logo,
                                                                     std::optional<KeyboardEffect> keys, GKeys gKeys)
{
    if (logo.has_value() && logo->kind() != KeyboardEffectKind::Static && logo->kind() != KeyboardEffectKind::Cycle &&
        logo->kind() != KeyboardEffectKind::Breathing) {
        return shared_kernel::fail(ErrorCode::InvalidArgument,
                                   "o logo só aceita cor fixa, ciclo de cores ou respiração");
    }
    if (keys.has_value() && keys->kind() == KeyboardEffectKind::Off) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "para apagar as teclas use cor fixa preta");
    }
    return G915ProfileSettings{logo, keys, gKeys};
}

shared_kernel::Result<std::vector<std::uint8_t>>
G915ProfileSettings::applyTo(std::span<const std::uint8_t> original, std::span<const std::uint8_t> baseline) const
{
    if (const auto ok = checkSector(original); !ok.has_value()) {
        return std::unexpected(ok.error());
    }
    if (baseline.size() != kSectorSize) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "cópia de referência com tamanho inesperado");
    }
    std::vector<std::uint8_t> sector{original.begin(), original.end()};

    for (std::size_t i = 0; i < kGKeyCount; ++i) {
        const auto& binding = gKeys_.at(i);
        std::array<std::uint8_t, 4> bytes = binding.encoded();
        switch (binding.kind()) {
        case GKeyBinding::Kind::Key:
            break;
        case GKeyBinding::Kind::Original:
            bytes = gKeyOf(baseline, i);
            break;
        case GKeyBinding::Kind::Raw:
            if (bytes != gKeyOf(original, i) && bytes != gKeyOf(baseline, i)) {
                return shared_kernel::fail(ErrorCode::InvalidArgument,
                                           "a tecla G" + std::to_string(i + 1) +
                                               " só aceita bytes que já estão no perfil ou na cópia");
            }
            break;
        }
        std::ranges::copy(bytes, sector.begin() + static_cast<std::ptrdiff_t>(kGKeysOffset + i * kGKeySize));
    }

    const auto editZone = [&](const std::optional<KeyboardEffect>& wanted, std::size_t primary,
                              std::size_t variant) -> shared_kernel::Result<void> {
        if (!wanted.has_value()) {
            return {};
        }
        const auto oldPrimary = entryOf(original, primary);
        const auto oldVariant = entryOf(original, variant);
        const auto newPrimary = encodeStoredEffect(*wanted, oldPrimary);
        if (!newPrimary.has_value()) {
            return std::unexpected(newPrimary.error());
        }
        putEntry(sector, primary, *newPrimary);
        putEntry(sector, variant, deriveDimmedVariant(oldPrimary, oldVariant, *newPrimary));
        return {};
    };
    if (const auto logo = editZone(logo_, kLogoEntry, kLogoVariant); !logo.has_value()) {
        return std::unexpected(logo.error());
    }
    if (const auto keys = editZone(keys_, kKeysEntry, kKeysVariant); !keys.has_value()) {
        return std::unexpected(keys.error());
    }
    return sector;
}

} // namespace ghub::lighting::domain
