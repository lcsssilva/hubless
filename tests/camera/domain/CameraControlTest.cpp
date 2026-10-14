#include "modules/camera/domain/CameraControl.hpp"

#include <gtest/gtest.h>

#include <string>

using ghub::camera::CameraControl;
using ghub::camera::CameraControlSpec;
using ghub::camera::ControlKind;
using ghub::camera::MenuOption;
using ghub::shared_kernel::ErrorCode;

namespace {

CameraControlSpec brightness()
{
    return {
        .id = 0x00980900,
        .name = "Brightness",
        .kind = ControlKind::Integer,
        .minimum = 0,
        .maximum = 255,
        .step = 1,
        .defaultValue = 128,
        .currentValue = 128,
        .inactive = false,
        .readOnly = false,
        .options = {},
    };
}

CameraControlSpec whiteBalanceAutomatic()
{
    return {
        .id = 0x0098090c,
        .name = "White Balance, Automatic",
        .kind = ControlKind::Boolean,
        .minimum = 0,
        .maximum = 1,
        .step = 1,
        .defaultValue = 1,
        .currentValue = 1,
        .inactive = false,
        .readOnly = false,
        .options = {},
    };
}

CameraControlSpec powerLine()
{
    return {
        .id = 0x00980918,
        .name = "Power Line Frequency",
        .kind = ControlKind::Menu,
        .minimum = 0,
        .maximum = 2,
        .step = 1,
        .defaultValue = 2,
        .currentValue = 2,
        .inactive = false,
        .readOnly = false,
        .options = {{.value = 0, .label = "Disabled"}, {.value = 1, .label = "50 Hz"}, {.value = 2, .label = "60 Hz"}},
    };
}

void expectRejected(CameraControlSpec spec, const std::string& expectedFragment)
{
    const auto name = spec.name;
    auto control = CameraControl::create(std::move(spec));
    ASSERT_FALSE(control.has_value());
    EXPECT_EQ(control.error().code, ErrorCode::InvalidArgument);
    EXPECT_NE(control.error().detail.find(name), std::string::npos) << control.error().detail;
    EXPECT_NE(control.error().detail.find(expectedFragment), std::string::npos) << control.error().detail;
}

} // namespace

TEST(CameraControl, FirmwareManaged_DefaultsToFalse_AndKeepsTheFlag)
{
    auto spec = brightness();
    EXPECT_FALSE(CameraControl::create(spec).value().firmwareManaged());

    spec.firmwareManaged = true;
    const auto control = CameraControl::create(spec).value();

    EXPECT_TRUE(control.firmwareManaged());
    EXPECT_TRUE(control.validateValue(100).has_value()); // gravar continua permitido
}

TEST(CameraControl, WithFirmwareManaged_ReturnsACopyWithOnlyTheFlagChanged)
{
    const auto original = CameraControl::create(brightness()).value();

    const auto marked = original.withFirmwareManaged(true);

    EXPECT_TRUE(marked.firmwareManaged());
    EXPECT_FALSE(original.firmwareManaged());
    EXPECT_EQ(marked.name(), original.name());
    EXPECT_EQ(marked.currentValue(), original.currentValue());
    EXPECT_FALSE(marked.withFirmwareManaged(false).firmwareManaged());
}

TEST(CameraControl, Accepts_Integer)
{
    auto control = CameraControl::create(brightness());

    ASSERT_TRUE(control.has_value());
    EXPECT_EQ(control->name(), "Brightness");
    EXPECT_EQ(control->kind(), ControlKind::Integer);
    EXPECT_EQ(control->minimum(), 0);
    EXPECT_EQ(control->maximum(), 255);
    EXPECT_EQ(control->step(), 1);
    EXPECT_EQ(control->defaultValue(), 128);
    EXPECT_EQ(control->currentValue(), std::optional<std::int64_t>{128});
    EXPECT_FALSE(control->inactive());
    EXPECT_FALSE(control->readOnly());
    EXPECT_TRUE(control->options().empty());
    EXPECT_FALSE(control->currentOption().has_value());
}

TEST(CameraControl, Accepts_Boolean)
{
    auto control = CameraControl::create(whiteBalanceAutomatic());

    ASSERT_TRUE(control.has_value());
    EXPECT_EQ(control->kind(), ControlKind::Boolean);
}

TEST(CameraControl, Accepts_MenuAndReportsCurrentOption)
{
    auto control = CameraControl::create(powerLine());

    ASSERT_TRUE(control.has_value());
    EXPECT_EQ(control->options().size(), 3U);
    EXPECT_EQ(control->currentOption(), (MenuOption{.value = 2, .label = "60 Hz"}));
}

TEST(CameraControl, Accepts_MenuWithSparseOptions)
{
    auto spec = powerLine();
    spec.name = "Auto Exposure";
    spec.minimum = 0;
    spec.maximum = 3;
    spec.defaultValue = 3;
    spec.currentValue = 3;
    spec.options = {{.value = 1, .label = "Manual Mode"}, {.value = 3, .label = "Aperture Priority Mode"}};

    auto control = CameraControl::create(spec);

    ASSERT_TRUE(control.has_value());
    EXPECT_EQ(control->currentOption(), (MenuOption{.value = 3, .label = "Aperture Priority Mode"}));
}

TEST(CameraControl, Accepts_MissingCurrentValue)
{
    auto spec = brightness();
    spec.currentValue = std::nullopt;

    auto control = CameraControl::create(spec);

    ASSERT_TRUE(control.has_value());
    EXPECT_FALSE(control->currentValue().has_value());
}

TEST(CameraControl, KeepsInactiveAndReadOnlyFlags)
{
    auto spec = brightness();
    spec.inactive = true;
    spec.readOnly = true;

    auto control = CameraControl::create(spec);

    ASSERT_TRUE(control.has_value());
    EXPECT_TRUE(control->inactive());
    EXPECT_TRUE(control->readOnly());
}

TEST(CameraControl, CurrentOptionIsEmpty_WhenNoOptionMatches)
{
    auto spec = powerLine();
    spec.name = "Auto Exposure";
    spec.maximum = 3;
    spec.currentValue = 2;
    spec.options = {{.value = 1, .label = "Manual Mode"}, {.value = 3, .label = "Aperture Priority Mode"}};

    auto control = CameraControl::create(spec);

    ASSERT_TRUE(control.has_value());
    EXPECT_FALSE(control->currentOption().has_value());
}

TEST(CameraControl, CurrentOptionIsEmpty_WhenMenuHasNoCurrentValue)
{
    auto spec = powerLine();
    spec.currentValue = std::nullopt;

    auto control = CameraControl::create(spec);

    ASSERT_TRUE(control.has_value());
    EXPECT_FALSE(control->currentOption().has_value());
}

TEST(CameraControl, Rejects_EmptyName)
{
    auto spec = brightness();
    spec.name.clear();

    auto control = CameraControl::create(spec);

    ASSERT_FALSE(control.has_value());
    EXPECT_EQ(control.error().code, ErrorCode::InvalidArgument);
}

TEST(CameraControl, Rejects_MinimumAboveMaximum)
{
    auto spec = brightness();
    spec.minimum = 300;
    spec.defaultValue = 300;
    spec.currentValue = 300;
    expectRejected(spec, "mínimo");
}

TEST(CameraControl, Rejects_StepBelowOne)
{
    auto spec = brightness();
    spec.step = 0;
    expectRejected(spec, "passo");
}

TEST(CameraControl, Rejects_DefaultOutOfRange)
{
    auto spec = brightness();
    spec.defaultValue = 256;
    expectRejected(spec, "padrão");
}

TEST(CameraControl, Rejects_CurrentValueOutOfRange)
{
    auto spec = brightness();
    spec.currentValue = -1;
    expectRejected(spec, "valor atual");
}

TEST(CameraControl, Rejects_BooleanWithWrongRange)
{
    auto spec = whiteBalanceAutomatic();
    spec.maximum = 2;
    expectRejected(spec, "0 a 1");
}

TEST(CameraControl, Rejects_MenuWithoutOptions)
{
    auto spec = powerLine();
    spec.options.clear();
    expectRejected(spec, "opção");
}

TEST(CameraControl, Rejects_MenuOptionOutOfRange)
{
    auto spec = powerLine();
    spec.options.push_back({.value = 3, .label = "Extra"});
    expectRejected(spec, "fora da faixa");
}

TEST(CameraControl, Rejects_MenuOptionWithEmptyLabel)
{
    auto spec = powerLine();
    spec.options[1].label.clear();
    expectRejected(spec, "rótulo");
}

TEST(CameraControl, Rejects_MenuOptionsWithRepeatedValues)
{
    auto spec = powerLine();
    spec.options.push_back({.value = 1, .label = "50 Hz again"});
    expectRejected(spec, "repetid");
}

TEST(CameraControl, Rejects_IntegerWithOptions)
{
    auto spec = brightness();
    spec.options = {{.value = 0, .label = "Low"}};
    expectRejected(spec, "opções");
}

TEST(CameraControl, Rejects_BooleanWithOptions)
{
    auto spec = whiteBalanceAutomatic();
    spec.options = {{.value = 0, .label = "Off"}};
    expectRejected(spec, "opções");
}

TEST(CameraControl, ComparesByValue)
{
    EXPECT_EQ(CameraControl::create(powerLine()).value(), CameraControl::create(powerLine()).value());
    auto other = powerLine();
    other.currentValue = 1;
    EXPECT_NE(CameraControl::create(powerLine()).value(), CameraControl::create(other).value());
}

// --- id opaco do backend ---

TEST(CameraControl, KeepsBackendId)
{
    auto control = CameraControl::create(brightness());

    ASSERT_TRUE(control.has_value());
    EXPECT_EQ(control->id(), 0x00980900U);
}

// --- validateValue ---

namespace {

CameraControlSpec autoExposure()
{
    auto spec = powerLine();
    spec.id = 0x009a0901;
    spec.name = "Auto Exposure";
    spec.maximum = 3;
    spec.defaultValue = 3;
    spec.currentValue = 3;
    spec.options = {{.value = 1, .label = "Manual Mode"}, {.value = 3, .label = "Aperture Priority Mode"}};
    return spec;
}

CameraControl built(CameraControlSpec spec)
{
    auto control = CameraControl::create(std::move(spec));
    EXPECT_TRUE(control.has_value());
    return *control;
}

void expectInvalidValue(const CameraControl& control, std::int64_t value, const std::string& fragment = {})
{
    const auto result = control.validateValue(value);
    ASSERT_FALSE(result.has_value()) << "valor " << value << " deveria ser recusado";
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_NE(result.error().detail.find(fragment), std::string::npos) << result.error().detail;
}

} // namespace

TEST(CameraControlValidateValue, AcceptsMinimumMaximumAndInside)
{
    const auto control = built(brightness());

    EXPECT_TRUE(control.validateValue(0).has_value());
    EXPECT_TRUE(control.validateValue(255).has_value());
    EXPECT_TRUE(control.validateValue(100).has_value());
}

TEST(CameraControlValidateValue, RejectsOutsideRange)
{
    const auto control = built(brightness());

    expectInvalidValue(control, -1, "faixa");
    expectInvalidValue(control, 256, "faixa");
}

TEST(CameraControlValidateValue, RejectsValueOffStep)
{
    auto spec = brightness();
    spec.minimum = 1;
    spec.maximum = 101;
    spec.step = 10;
    spec.defaultValue = 51;
    spec.currentValue = 51;
    const auto control = built(spec);

    EXPECT_TRUE(control.validateValue(1).has_value());
    EXPECT_TRUE(control.validateValue(11).has_value());
    EXPECT_TRUE(control.validateValue(101).has_value());
    expectInvalidValue(control, 2, "passo");
    expectInvalidValue(control, 10, "passo");
    expectInvalidValue(control, 12, "passo");
}

TEST(CameraControlValidateValue, AcceptsOnlyZeroOrOne_ForBoolean)
{
    const auto control = built(whiteBalanceAutomatic());

    EXPECT_TRUE(control.validateValue(0).has_value());
    EXPECT_TRUE(control.validateValue(1).has_value());
    expectInvalidValue(control, 2);
    expectInvalidValue(control, -1);
}

TEST(CameraControlValidateValue, AcceptsOnlyEnumeratedItems_ForMenu)
{
    const auto control = built(autoExposure());

    EXPECT_TRUE(control.validateValue(1).has_value());
    EXPECT_TRUE(control.validateValue(3).has_value());
    expectInvalidValue(control, 0, "opção");
    expectInvalidValue(control, 2, "opção");
    expectInvalidValue(control, 4);
}

TEST(CameraControlValidateValue, RejectsReadOnlyControl)
{
    auto spec = brightness();
    spec.readOnly = true;

    expectInvalidValue(built(spec), 100, "somente leitura");
}

TEST(CameraControlValidateValue, RejectsInactiveControl)
{
    auto spec = brightness();
    spec.inactive = true;

    expectInvalidValue(built(spec), 100, "inativo");
}
