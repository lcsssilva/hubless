#include "modules/headset/infrastructure/HidppHeadsetSettingsWriter.hpp"
#include "fakes/StatusReadHarness.hpp"
#include "modules/headset/infrastructure/G733StatusPolicy.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

namespace ghub::headset::infrastructure {
namespace {

using platform::hidpp::HidppNodeLocator;
using shared_kernel::ErrorCode;
using Bytes = std::vector<std::uint8_t>;

// Escritas: os parâmetros não são nulos, então o TearDown do harness (que exige parâmetros nulos) é substituído por
// uma conferência de cada quadro, com os parâmetros completos, contra a política do módulo.
class HidppHeadsetSettingsWriterTest : public fakes::StatusReadFixture {
protected:
    void SetUp() override
    {
        StatusReadFixture::SetUp();
        // Mapa de features das capturas C-00/C-01 do G733 (os índices são resolvidos pela Root, nunca fixos).
        device.features = {0x0001, 0x0003, 0x0005, 0x8070, 0x8010, 0x8310, 0x8300, 0x1f20};
    }

    void TearDown() override
    {
        std::filesystem::remove_all(root);
        EXPECT_EQ(openHandles, 0) << "o hidraw ficou aberto depois da escrita";
        for (const auto& frame : io->written()) {
            ASSERT_EQ(frame.size(), 20U);
            ASSERT_EQ(frame[1], 0xFF) << "índice de dispositivo do G733";
            ASSERT_EQ(frame[3] & 0x0F, 0x0B);
            if (frame[2] == 0) {
                EXPECT_TRUE(frameAllowed(0, 0, {frame[4], frame[5]}));
                continue;
            }
            const auto feature = device.features.at(frame[2] - 1U);
            const std::uint8_t function = frame[3] >> 4;
            // Menor prefixo aceito pela política: o adaptador não pode enviar nada fora dela.
            bool accepted = false;
            for (std::ptrdiff_t size = 0; size <= 16 && !accepted; ++size) {
                accepted = frameAllowed(feature, function,
                                        std::vector<std::uint8_t>(frame.begin() + 4, frame.begin() + 4 + size));
            }
            EXPECT_TRUE(accepted) << "quadro fora da política: " << std::hex << feature << " fn " << std::dec
                                  << int{function};
        }
    }

    bool frameAllowed(std::uint16_t featureId, std::uint8_t function,
                      const std::vector<std::uint8_t>& params) const override
    {
        return G733StatusPolicy{}.isAllowed(featureId, function, params);
    }

    [[nodiscard]] HidppHeadsetSettingsWriter writer()
    {
        return HidppHeadsetSettingsWriter{HidppNodeLocator{root, "/dev"}, clock, opener()};
    }

    // Parâmetros (4..19) do quadro i, cortados em `size` bytes.
    [[nodiscard]] Bytes params(std::size_t frameIndex, std::size_t size) const
    {
        const auto& frame = io->written().at(frameIndex);
        return Bytes(frame.begin() + 4, frame.begin() + 4 + static_cast<std::ptrdiff_t>(size));
    }
};

TEST_F(HidppHeadsetSettingsWriterTest, SidetoneIsOneByteOnFunctionOneOfTheFeature8300)
{
    auto settings = writer();

    const auto done = settings.applySidetone(SidetoneLevel::create(50).value());

    ASSERT_TRUE(done.has_value()) << done.error().detail;
    EXPECT_EQ(sequence(), (std::vector<std::string>{"root 8300", "8300 fn1"}));
    EXPECT_EQ(params(1, 1), (Bytes{0x32})) << "C-01: 0x8300 fn1 32";
    EXPECT_EQ(openedPaths, (std::vector<std::filesystem::path>{"/dev/hidraw3"}));
}

TEST_F(HidppHeadsetSettingsWriterTest, SidetoneZeroAndOneHundredAreSent)
{
    auto settings = writer();

    ASSERT_TRUE(settings.applySidetone(SidetoneLevel::create(0).value()).has_value());
    ASSERT_TRUE(settings.applySidetone(SidetoneLevel::create(100).value()).has_value());

    EXPECT_EQ(params(1, 1), (Bytes{0x00}));
    EXPECT_EQ(params(3, 1), (Bytes{0x64}));
}

TEST_F(HidppHeadsetSettingsWriterTest, LightsOnIsTheCycleOnBothZonesWithOneWriteEach)
{
    auto settings = writer();

    const auto done = settings.applyLights(HeadsetLights::On);

    ASSERT_TRUE(done.has_value()) << done.error().detail;
    EXPECT_EQ(sequence(), (std::vector<std::string>{"root 8070", "8070 fn3", "8070 fn3"}));
    EXPECT_EQ(params(1, 10), (Bytes{0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x88, 0x64})) << "C-01 zona 0";
    EXPECT_EQ(params(2, 10), (Bytes{0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x88, 0x64})) << "C-01 zona 1";
}

TEST_F(HidppHeadsetSettingsWriterTest, LightsOffIsEffectZeroOnBothZones)
{
    auto settings = writer();

    ASSERT_TRUE(settings.applyLights(HeadsetLights::Off).has_value());

    EXPECT_EQ(sequence(), (std::vector<std::string>{"root 8070", "8070 fn3", "8070 fn3"}));
    EXPECT_EQ(params(1, 2), (Bytes{0x00, 0x00}));
    EXPECT_EQ(params(2, 2), (Bytes{0x01, 0x00}));
}

TEST_F(HidppHeadsetSettingsWriterTest, ApplyLightingSendsTheBytesOfTheValueObjectInOneWrite)
{
    auto settings = writer();
    const auto lighting =
        HeadsetLighting::create(LightingZone::Upper, LightingEffect::Breathing, {0x00, 0xFF, 0xFF}, 5000, 100);

    const auto done = settings.applyLighting(lighting.value());

    ASSERT_TRUE(done.has_value()) << done.error().detail;
    EXPECT_EQ(sequence(), (std::vector<std::string>{"root 8070", "8070 fn3"}));
    EXPECT_EQ(params(1, 9), (Bytes{0x01, 0x02, 0x00, 0xFF, 0xFF, 0x13, 0x88, 0x00, 0x64})) << "C-03 respiração";
}

TEST_F(HidppHeadsetSettingsWriterTest, ErrorFiveMeansTheHeadsetIsOffOrOutOfRange)
{
    device.hidpp20Error = 0x05;
    auto settings = writer();

    const auto done = settings.applySidetone(SidetoneLevel::create(10).value());

    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(done.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(done.error().detail, "headset desligado ou fora de alcance");
    EXPECT_EQ(openHandles, 0);
}

TEST_F(HidppHeadsetSettingsWriterTest, LightsStopAtTheFirstFailure)
{
    device.hidpp20Error = 0x05;
    auto settings = writer();

    ASSERT_FALSE(settings.applyLights(HeadsetLights::On).has_value());

    EXPECT_EQ(io->written().size(), 1U) << "só a consulta Root; nada mais é tentado";
}

TEST_F(HidppHeadsetSettingsWriterTest, TimesOutAfterTwoSecondsWithoutResending)
{
    device.silent = true;
    auto settings = writer();
    const auto before = clock.now();

    const auto done = settings.applySidetone(SidetoneLevel::create(10).value());

    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(clock.now() - before, std::chrono::milliseconds{2000});
    EXPECT_EQ(io->written().size(), 1U);
}

TEST_F(HidppHeadsetSettingsWriterTest, MissingFeatureFailsWithoutWriting)
{
    device.features = {0x0001, 0x8310};
    auto settings = writer();

    const auto done = settings.applyLights(HeadsetLights::Off);

    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(sequence(), (std::vector<std::string>{"root 8070"}));
}

TEST_F(HidppHeadsetSettingsWriterTest, FailsWithoutOpeningWhenTheNodeIsNotFound)
{
    std::filesystem::remove_all(root / "class" / "hidraw" / "hidraw3");
    auto settings = writer();

    ASSERT_FALSE(settings.applySidetone(SidetoneLevel::create(10).value()).has_value());
    EXPECT_TRUE(openedPaths.empty());
}

} // namespace
} // namespace ghub::headset::infrastructure
