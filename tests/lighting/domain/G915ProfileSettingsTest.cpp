#include "modules/lighting/domain/G915ProfileSettings.hpp"

#include "fakes/G915SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <vector>

// Perfil onboard do G915 (INC-019 R4, INC-022 R1/R4): efeitos gravados e teclas G1-G5, conferidos byte a byte com as
// cópias reais da memória lidas em 2026-10-15.
namespace {

using namespace ghub::lighting::domain;
using ghub::shared_kernel::ErrorCode;
namespace fx = ghub::fixtures::g915;
using Bytes = std::vector<std::uint8_t>;

template <std::size_t N> Bytes bytes(const std::array<std::uint8_t, N>& array)
{
    return {array.begin(), array.end()};
}

KeyId key(const char* name)
{
    return *G915KeyCatalog::findByName(name); // NOLINT(bugprone-unchecked-optional-access)
}

EffectPeriod period(std::uint32_t ms)
{
    return EffectPeriod::create(ms).value();
}
EffectBrightness brightness(std::uint32_t percent)
{
    return EffectBrightness::create(percent).value();
}

std::set<std::size_t> differences(const Bytes& left, const Bytes& right)
{
    std::set<std::size_t> positions;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (left[i] != right[i]) {
            positions.insert(i);
        }
    }
    return positions;
}

G915ProfileSettings readOf(const Bytes& sector)
{
    return G915ProfileSettings::read(sector).value();
}

G915ProfileSettings::GKeys keepAll(const G915ProfileSettings& settings)
{
    return settings.gKeys();
}

} // namespace

TEST(GKeyBinding, CtrlPlusCIsExactlyTheBytesTheGHubWrote)
{
    const auto binding = GKeyBinding::create(GKeyBinding::kCtrl, key("Key: C"));

    ASSERT_TRUE(binding.has_value());
    // C-06: G1 com "Copy" (Ctrl+C) gravou `80 02 01 06` no offset 0x20 do M1.
    EXPECT_EQ(binding->encoded(), (std::array<std::uint8_t, 4>{0x80, 0x02, 0x01, 0x06}));
}

TEST(GKeyBinding, CombinesTheModifierBitmap)
{
    const auto binding = GKeyBinding::create(GKeyBinding::kCtrl | GKeyBinding::kShift | GKeyBinding::kAlt |
                                                 GKeyBinding::kSuper,
                                             key("Key: F1"));

    ASSERT_TRUE(binding.has_value());
    EXPECT_EQ(binding->encoded(), (std::array<std::uint8_t, 4>{0x80, 0x02, 0x0F, 0x3A}));
}

TEST(GKeyBinding, RefusesModifiersOutsideTheBitmap)
{
    const auto binding = GKeyBinding::create(0x10, key("Key: C"));

    ASSERT_FALSE(binding.has_value());
    EXPECT_EQ(binding.error().code, ErrorCode::InvalidArgument);
}

TEST(GKeyBinding, RefusesKeysThatAreNotKeyboardKeysOfTheCatalog)
{
    EXPECT_FALSE(GKeyBinding::create(0, key("Key: G1")).has_value());
    EXPECT_FALSE(GKeyBinding::create(0, key("Logo")).has_value());
    EXPECT_FALSE(GKeyBinding::create(0, key("Key: Left Control")).has_value());
}

TEST(GKeyBinding, DecodesTheRealSectorValues)
{
    const auto f1 = GKeyBinding::decode({0x80, 0x02, 0x00, 0x3A});
    EXPECT_EQ(f1.kind(), GKeyBinding::Kind::Key);
    EXPECT_EQ(f1.modifiers(), 0);
    EXPECT_EQ(f1.key(), key("Key: F1"));

    const auto copy = GKeyBinding::decode({0x80, 0x02, 0x01, 0x06});
    EXPECT_EQ(copy.kind(), GKeyBinding::Kind::Key);
    EXPECT_EQ(copy.modifiers(), GKeyBinding::kCtrl);
    EXPECT_EQ(copy.key(), key("Key: C"));
}

TEST(GKeyBinding, OtherFormatsAreRawAndKeepTheirBytes)
{
    const std::array<std::uint8_t, 4> media{0x90, 0x0D, 0xFF, 0x01};
    const auto raw = GKeyBinding::decode(media);

    EXPECT_EQ(raw.kind(), GKeyBinding::Kind::Raw);
    EXPECT_EQ(raw.encoded(), media);
    // Tecla fora do catálogo de teclado (0x01 não é uma tecla) também fica bruta.
    EXPECT_EQ(GKeyBinding::decode({0x80, 0x02, 0x00, 0x01}).kind(), GKeyBinding::Kind::Raw);
    // Modificadores fora do bitmap também.
    EXPECT_EQ(GKeyBinding::decode({0x80, 0x02, 0x20, 0x3A}).kind(), GKeyBinding::Kind::Raw);
}

TEST(G915ProfileSettings, ReadsTheOriginalM1)
{
    const auto settings = G915ProfileSettings::read(bytes(fx::kBeforeC04M1));

    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->logo(), KeyboardEffect::staticColor(RgbColor{0, 0, 0}));
    EXPECT_EQ(settings->keys(), KeyboardEffect::ripple(RgbColor{0, 0, 0}, RippleSpeed::create(20).value()));
    const std::array<const char*, 5> expected{"Key: F1", "Key: F2", "Key: F3", "Key: F4", "Key: F5"};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(settings->gKeys().at(i).kind(), GKeyBinding::Kind::Key) << i;
        EXPECT_EQ(settings->gKeys().at(i).key(), key(expected.at(i))) << i;
        EXPECT_EQ(settings->gKeys().at(i).modifiers(), 0) << i;
    }
}

TEST(G915ProfileSettings, ReadsG1AsCtrlPlusCFromTheGHubProfile)
{
    const auto settings = readOf(bytes(fx::kAfterC06M1));

    EXPECT_EQ(settings.gKeys().at(0).modifiers(), GKeyBinding::kCtrl);
    EXPECT_EQ(settings.gKeys().at(0).key(), key("Key: C"));
    EXPECT_EQ(settings.logo(), KeyboardEffect::cycle(period(5000), brightness(100)));
    EXPECT_EQ(settings.keys(), KeyboardEffect::wave(period(5000), brightness(100)));
}

TEST(G915ProfileSettings, ReadingRefusesUnexpectedSectors)
{
    EXPECT_FALSE(G915ProfileSettings::read(Bytes(254, 0)).has_value());
    EXPECT_FALSE(G915ProfileSettings::read(Bytes{}).has_value());
    auto otherFormat = bytes(fx::kBeforeC04M1);
    otherFormat[0] = 0x02;
    const auto refused = G915ProfileSettings::read(otherFormat);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, ErrorCode::MalformedData);
}

TEST(G915ProfileSettings, RewritingTheReadValuesChangesNothingInAnyRealSector)
{
    for (const Bytes& sector :
         {bytes(fx::kBeforeC04M1), bytes(fx::kBeforeC04M2), bytes(fx::kBeforeC04M3), bytes(fx::kAfterC06M1),
          bytes(fx::kAfterRestoreM1), bytes(fx::kFinalM1)}) {
        const auto settings = readOf(sector);
        const auto written = settings.applyTo(sector, sector);
        ASSERT_TRUE(written.has_value());
        EXPECT_EQ(*written, sector);
    }
}

TEST(G915ProfileSettings, SettingG1ToCtrlPlusCChangesOnlyTheTwoModifierAndKeyBytes)
{
    const auto original = bytes(fx::kBeforeC04M1);
    const auto current = readOf(original);
    auto gKeys = keepAll(current);
    gKeys.at(0) = GKeyBinding::create(GKeyBinding::kCtrl, key("Key: C")).value();
    const auto edited = G915ProfileSettings::make(current.logo(), current.keys(), gKeys).value();

    const auto written = edited.applyTo(original, original);

    ASSERT_TRUE(written.has_value());
    // Offset 0x20 = G1: `80 02 00 3a` -> `80 02 01 06` (C-06).
    EXPECT_EQ(differences(original, *written), (std::set<std::size_t>{0x22, 0x23}));
    EXPECT_EQ((Bytes{written->begin() + 0x20, written->begin() + 0x24}), (Bytes{0x80, 0x02, 0x01, 0x06}));
}

TEST(G915ProfileSettings, TheOtherGKeysKeepTheirBytes)
{
    const auto original = bytes(fx::kBeforeC04M1);
    const auto current = readOf(original);
    auto gKeys = keepAll(current);
    gKeys.at(2) = GKeyBinding::create(GKeyBinding::kShift | GKeyBinding::kAlt, key("Key: Z")).value();
    const auto written =
        G915ProfileSettings::make(current.logo(), current.keys(), gKeys).value().applyTo(original, original);

    ASSERT_TRUE(written.has_value());
    for (const auto& position : differences(original, *written)) {
        EXPECT_TRUE(position >= 0x28 && position < 0x2C) << position; // só a G3
    }
}

TEST(G915ProfileSettings, OriginalBindingRestoresTheBaselineBytes)
{
    // Estado atual: G1 = Ctrl+C (perfil do G HUB). Cópia original (baseline): G1 = F1.
    const auto current = bytes(fx::kAfterC06M1);
    const auto baseline = bytes(fx::kBeforeC04M1);
    const auto settings = readOf(current);
    auto gKeys = keepAll(settings);
    gKeys.at(0) = GKeyBinding::original();
    const auto written = G915ProfileSettings::make(settings.logo(), settings.keys(), gKeys).value().applyTo(current, baseline);

    ASSERT_TRUE(written.has_value());
    EXPECT_EQ((Bytes{written->begin() + 0x20, written->begin() + 0x24}), (Bytes{0x80, 0x02, 0x00, 0x3A}));
    EXPECT_EQ(differences(current, *written), (std::set<std::size_t>{0x22, 0x23}));
}

TEST(G915ProfileSettings, RawBindingsAreOnlyAcceptedWhenTheyMatchTheSectorOrTheBaseline)
{
    const auto original = bytes(fx::kBeforeC04M1);
    const auto settings = readOf(original);
    auto gKeys = keepAll(settings);

    gKeys.at(1) = GKeyBinding::raw({0x90, 0x0D, 0xFF, 0x01}); // não está no setor nem na cópia
    const auto injected = G915ProfileSettings::make(settings.logo(), settings.keys(), gKeys).value().applyTo(original, original);
    ASSERT_FALSE(injected.has_value());
    EXPECT_EQ(injected.error().code, ErrorCode::InvalidArgument);

    gKeys.at(1) = GKeyBinding::raw({0x80, 0x02, 0x00, 0x3B}); // o que o setor já tem em G2
    const auto same = G915ProfileSettings::make(settings.logo(), settings.keys(), gKeys).value().applyTo(original, original);
    ASSERT_TRUE(same.has_value());
    EXPECT_EQ(*same, original);
}

TEST(G915ProfileSettings, EditingBothEffectsReproducesTheLightingBlockTheGHubWrote)
{
    const auto original = bytes(fx::kBeforeC04M1);
    const auto target = bytes(fx::kAfterC06M1);
    const auto current = readOf(original);

    const auto edited = G915ProfileSettings::make(KeyboardEffect::cycle(period(5000), brightness(100)),
                                                  KeyboardEffect::wave(period(5000), brightness(100)),
                                                  current.gKeys())
                            .value();
    const auto written = edited.applyTo(original, original);

    ASSERT_TRUE(written.has_value());
    // Bloco 0xD0..0xFB (4 entradas de 11 bytes) idêntico ao do M1 que o G HUB gravou; nada fora dele mudou.
    EXPECT_EQ((Bytes{written->begin() + 0xD0, written->begin() + 0xFC}), (Bytes{target.begin() + 0xD0, target.begin() + 0xFC}));
    for (const auto& position : differences(original, *written)) {
        EXPECT_TRUE(position >= 0xD0 && position < 0xFC) << position;
    }
}

TEST(G915ProfileSettings, ChangingTheRippleSpeedChangesTheTwoSpeedBytesLikeTheGHubDid)
{
    const auto original = bytes(fx::kBeforeC04M1);
    const auto current = readOf(original);
    const auto edited = G915ProfileSettings::make(current.logo(),
                                                  KeyboardEffect::ripple(RgbColor{0, 0, 0}, RippleSpeed::create(2).value()),
                                                  current.gKeys())
                            .value();

    const auto written = edited.applyTo(original, original);

    ASSERT_TRUE(written.has_value());
    // Final: 0xe1 e 0xf7 de 0x14 para 0x02 (evidência 2026-10-15).
    EXPECT_EQ(differences(original, *written), (std::set<std::size_t>{0xE1, 0xF7}));
    EXPECT_EQ((*written)[0xE1], 0x02);
    EXPECT_EQ((*written)[0xF7], 0x02);
}

TEST(G915ProfileSettings, AnEffectThatIsNotSetKeepsItsRawEntries)
{
    auto original = bytes(fx::kBeforeC04M1);
    original[0xD0] = 0x05; // entrada 0 num formato sem evidência
    original[0xE6] = 0x05; // entrada 2 idem
    const auto settings = readOf(original);
    ASSERT_FALSE(settings.logo().has_value());

    const auto edited = G915ProfileSettings::make(std::nullopt, settings.keys(), settings.gKeys()).value();
    const auto written = edited.applyTo(original, original);

    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(*written, original);
}

TEST(G915ProfileSettings, TheLogoOnlyAcceptsStaticCycleAndBreathing)
{
    const auto wave = KeyboardEffect::wave(period(2100), brightness(100));
    const auto ripple = KeyboardEffect::ripple(RgbColor{0, 0, 0}, RippleSpeed::create(20).value());
    const auto keys = KeyboardEffect::staticColor(RgbColor{1, 2, 3});
    const auto gKeys = readOf(bytes(fx::kBeforeC04M1)).gKeys();

    for (const auto& unsupported : {wave, ripple, KeyboardEffect::off()}) {
        const auto made = G915ProfileSettings::make(unsupported, keys, gKeys);
        ASSERT_FALSE(made.has_value());
        EXPECT_EQ(made.error().code, ErrorCode::InvalidArgument);
    }
    EXPECT_TRUE(G915ProfileSettings::make(KeyboardEffect::cycle(period(5000), brightness(100)), keys, gKeys).has_value());
    EXPECT_TRUE(G915ProfileSettings::make(KeyboardEffect::staticColor(RgbColor{1, 2, 3}), keys, gKeys).has_value());
    EXPECT_TRUE(G915ProfileSettings::make(
                    KeyboardEffect::breathing(RgbColor{1, 2, 3}, period(3000), brightness(100)), keys, gKeys)
                    .has_value());
}

TEST(G915ProfileSettings, TheKeysRefuseOff)
{
    const auto gKeys = readOf(bytes(fx::kBeforeC04M1)).gKeys();

    const auto made = G915ProfileSettings::make(std::nullopt, KeyboardEffect::off(), gKeys);

    ASSERT_FALSE(made.has_value());
    EXPECT_EQ(made.error().code, ErrorCode::InvalidArgument);
}

TEST(G915ProfileSettings, ApplyingRefusesASectorOfAnotherSizeOrFormat)
{
    const auto original = bytes(fx::kBeforeC04M1);
    const auto settings = readOf(original);

    EXPECT_FALSE(settings.applyTo(Bytes(256, 0), original).has_value());
    auto otherFormat = original;
    otherFormat[0] = 0x07;
    EXPECT_FALSE(settings.applyTo(otherFormat, original).has_value());
    EXPECT_FALSE(settings.applyTo(original, Bytes(10, 0)).has_value());
}
