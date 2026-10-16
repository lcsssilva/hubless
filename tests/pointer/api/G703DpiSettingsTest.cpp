#include "modules/pointer/api/G703DpiSettings.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeOnboardMemory.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

// INC-017 parte 2 (R10): fachada do módulo pointer para a seção de DPI do perfil. O caminho seguro (cópia, releitura,
// restauração) é do G703OnboardSettings; aqui só a conversão, a preservação dos LEDs e a propagação de erros.
namespace ghub::pointer {
namespace {

using Bytes = std::vector<std::uint8_t>;
using shared_kernel::ErrorCode;

class G703DpiSettingsTest : public ::testing::Test {
protected:
    G703DpiSettingsTest()
    {
        connector.state.sectors[0] = Bytes{fixtures::kDirectory0000.begin(), fixtures::kDirectory0000.end()};
        connector.state.sectors[1] = Bytes{fixtures::kSector0001.begin(), fixtures::kSector0001.end()};
        connector.state.sectors[2] = Bytes{fixtures::kSector0002.begin(), fixtures::kSector0002.end()};
    }

    [[nodiscard]] bool wroteNothing() const
    {
        return std::ranges::none_of(connector.state.calls,
                                    [](const std::string& call) { return call.starts_with("write:"); });
    }

    fakes::FakeClock clock;
    device_access::AccessRegistry registry{clock};
    fakes::FakeExternalProcessProbe probe;
    fakes::FakeOnboardMemoryConnector connector;
    fakes::FakeOnboardBackupStore backup;
    G703OnboardSettings onboard{probe, registry, connector, backup};
    G703DpiSettings facade{onboard};
};

TEST_F(G703DpiSettingsTest, ReadsTheActiveProfileWithoutWriting)
{
    const auto current = facade.readCurrent();
    ASSERT_TRUE(current.has_value());
    EXPECT_EQ(current->reportRateHz, 1000);
    EXPECT_EQ(current->dpiStages, std::vector<int>{7050});
    EXPECT_EQ(current->defaultStage, 0);
    EXPECT_EQ(current->dpiShiftStage, 0);
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G703DpiSettingsTest, AppliesRateAndStagesThroughTheSafeWriteAndKeepsTheLeds)
{
    // LEDs diferentes do original: a aplicação do perfil não pode tocá-los.
    ASSERT_TRUE(onboard
                    .write(G703ProfileSettings::make(1000, {7050}, 0, 0, LedSetting::fixed(0, 255, 255),
                                                     *LedSetting::cycle(5000, 80))
                               .value())
                    .has_value());
    const G703DpiValue wanted{.reportRateHz = 500, .dpiStages = {400, 1600, 3200}, .defaultStage = 2,
                              .dpiShiftStage = 1};
    const auto applied = facade.apply(wanted);
    ASSERT_TRUE(applied.has_value());
    EXPECT_EQ(applied->value, wanted);
    EXPECT_TRUE(applied->wrote);
    const auto after = onboard.read();
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->settings.primaryLed(), LedSetting::fixed(0, 255, 255));
    EXPECT_EQ(after->settings.logoLed(), *LedSetting::cycle(5000, 80));
    EXPECT_TRUE(after->backupExists); // a cópia automática do caminho seguro foi feita
}

TEST_F(G703DpiSettingsTest, ValuesAlreadyOnTheMouseSendNothing)
{
    const auto current = facade.readCurrent().value();
    const auto applied = facade.apply(current);
    ASSERT_TRUE(applied.has_value());
    EXPECT_FALSE(applied->wrote);
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G703DpiSettingsTest, NoShiftInTheProfileKeepsTheCurrentShift)
{
    const G703DpiValue wanted{.reportRateHz = 1000, .dpiStages = {800, 1600}, .defaultStage = 0,
                              .dpiShiftStage = std::nullopt};
    const auto applied = facade.apply(wanted);
    ASSERT_TRUE(applied.has_value());
    EXPECT_EQ(applied->value.dpiStages, (std::vector<int>{800, 1600}));
    EXPECT_EQ(applied->value.dpiShiftStage, 0); // o do mouse continua
}

TEST_F(G703DpiSettingsTest, RejectsInvalidValuesWithoutWriting)
{
    EXPECT_EQ(facade.apply({.reportRateHz = 333, .dpiStages = {800}, .defaultStage = 0, .dpiShiftStage = 0})
                  .error()
                  .code,
              ErrorCode::InvalidArgument);
    EXPECT_EQ(facade.apply({.reportRateHz = 1000, .dpiStages = {75}, .defaultStage = 0, .dpiShiftStage = 0})
                  .error()
                  .code,
              ErrorCode::InvalidArgument);
    EXPECT_EQ(facade.apply({.reportRateHz = 1000, .dpiStages = {800}, .defaultStage = 3, .dpiShiftStage = 0})
                  .error()
                  .code,
              ErrorCode::InvalidArgument);
    // Estágio fora de uint16_t não pode virar outro valor por truncamento.
    EXPECT_EQ(facade.apply({.reportRateHz = 1000, .dpiStages = {70000}, .defaultStage = 0, .dpiShiftStage = 0})
                  .error()
                  .code,
              ErrorCode::InvalidArgument);
    EXPECT_TRUE(wroteNothing());
}

TEST_F(G703DpiSettingsTest, OwnerRefusalIsPropagated)
{
    probe.setResult(std::vector<device_access::ExternalOwner>{device_access::ExternalOwner::Ratbagd});
    const auto applied = facade.apply({.reportRateHz = 500, .dpiStages = {800}, .defaultStage = 0, .dpiShiftStage = 0});
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(applied.error().detail, "em uso por ratbagd");
    EXPECT_FALSE(facade.readCurrent().has_value());
}

} // namespace
} // namespace ghub::pointer
