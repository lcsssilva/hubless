#include "modules/lighting/api/KeyboardEffectSettings.hpp"

#include <gtest/gtest.h>

#include <vector>

// INC-017 parte 2 (R10): fachada do módulo lighting para a seção de efeito do G915 no perfil.

namespace ghub::lighting {
namespace {

using shared_kernel::ErrorCode;

class RecordingApplier final : public KeyboardEffectApplier {
public:
    [[nodiscard]] shared_kernel::Result<void> applyEffect(const KeyboardEffectValue& value) override
    {
        requested.push_back(value);
        return result;
    }

    std::vector<KeyboardEffectValue> requested;
    shared_kernel::Result<void> result;
};

class KeyboardEffectSettingsTest : public ::testing::Test {
protected:
    static KeyboardEffectValue breathing()
    {
        return {.kind = "breathing", .red = 255, .green = 0, .blue = 64, .brightness = 60};
    }

    RecordingApplier applier;
    KeyboardEffectSettings facade{applier};
};

TEST_F(KeyboardEffectSettingsTest, ForwardsAValidEffectToTheLiveSessionAndRemembersIt)
{
    EXPECT_FALSE(facade.last().has_value());
    const auto applied = facade.apply(breathing());
    ASSERT_TRUE(applied.has_value());
    EXPECT_EQ(*applied, breathing());
    ASSERT_EQ(applier.requested.size(), 1U);
    EXPECT_EQ(applier.requested[0], breathing());
    EXPECT_EQ(facade.last(), breathing());
}

TEST_F(KeyboardEffectSettingsTest, AcceptsEveryKnownKind)
{
    for (const char* kind : {"wave", "cycle", "breathing", "ripple", "static"}) {
        auto value = breathing();
        value.kind = kind;
        EXPECT_TRUE(facade.apply(value).has_value()) << kind;
    }
    EXPECT_EQ(applier.requested.size(), 5U);
}

TEST_F(KeyboardEffectSettingsTest, RejectsUnknownKindColorAndBrightnessWithoutForwarding)
{
    auto bad = breathing();
    bad.kind = "disco";
    EXPECT_EQ(facade.apply(bad).error().code, ErrorCode::InvalidArgument);
    bad = breathing();
    bad.red = 256;
    EXPECT_EQ(facade.apply(bad).error().code, ErrorCode::InvalidArgument);
    bad = breathing();
    bad.green = -1;
    EXPECT_EQ(facade.apply(bad).error().code, ErrorCode::InvalidArgument);
    bad = breathing();
    bad.brightness = 0;
    EXPECT_EQ(facade.apply(bad).error().code, ErrorCode::InvalidArgument);
    bad = breathing();
    bad.brightness = 101;
    EXPECT_EQ(facade.apply(bad).error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(applier.requested.empty());
    EXPECT_FALSE(facade.last().has_value());
}

TEST_F(KeyboardEffectSettingsTest, RefusalFromTheSessionIsReportedAndNotRemembered)
{
    applier.result = shared_kernel::fail(ErrorCode::DeviceBusy, "teclado ocupado");
    const auto applied = facade.apply(breathing());
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(applied.error().code, ErrorCode::DeviceBusy);
    EXPECT_FALSE(facade.last().has_value());
}

TEST_F(KeyboardEffectSettingsTest, RememberDoesNotTouchTheKeyboard)
{
    facade.remember(breathing());
    EXPECT_EQ(facade.last(), breathing());
    EXPECT_TRUE(applier.requested.empty());
}

} // namespace
} // namespace ghub::lighting
