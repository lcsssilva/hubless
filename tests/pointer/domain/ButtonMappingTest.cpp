#include "modules/pointer/domain/ButtonMapping.hpp"

#include <gtest/gtest.h>

using ghub::pointer::ButtonAction;
using ghub::pointer::ButtonActionKind;
using ghub::pointer::ButtonMapping;
using ghub::pointer::ButtonMappingSpec;
using ghub::shared_kernel::ErrorCode;

namespace {

// Botão b5 do G703 (evidence): Mapping (2, 1073741831), ActionTypes 0 1 2 3 4.
ButtonMappingSpec g703Button5()
{
    return {
        .index = 5,
        .action = ButtonAction::special(1073741831U),
        .supportedKinds =
            {
                ButtonActionKind::None,
                ButtonActionKind::Button,
                ButtonActionKind::Special,
                ButtonActionKind::Key,
                ButtonActionKind::Macro,
            },
    };
}

} // namespace

TEST(ButtonMappingTest, CreatesTheRealSpecialButton)
{
    const auto mapping = ButtonMapping::create(g703Button5());

    ASSERT_TRUE(mapping.has_value()) << mapping.error().detail;
    EXPECT_EQ(mapping->index(), 5U);
    EXPECT_EQ(mapping->action().kind(), ButtonActionKind::Special);
    EXPECT_EQ(mapping->action().value(), 1073741831U);
    EXPECT_EQ(mapping->supportedKinds().size(), 5U);
}

TEST(ButtonMappingTest, ActionFactoriesSetKindAndValue)
{
    EXPECT_EQ(ButtonAction::none().kind(), ButtonActionKind::None);
    EXPECT_EQ(ButtonAction::none().value(), 0U);
    EXPECT_EQ(ButtonAction::button(3).kind(), ButtonActionKind::Button);
    EXPECT_EQ(ButtonAction::button(3).value(), 3U);
    EXPECT_EQ(ButtonAction::key(30).kind(), ButtonActionKind::Key);
    EXPECT_EQ(ButtonAction::key(30).value(), 30U);
    EXPECT_EQ(ButtonAction::macro(3).kind(), ButtonActionKind::Macro);
    EXPECT_EQ(ButtonAction::macro(3).value(), 3U);
    EXPECT_EQ(ButtonAction::unknown().kind(), ButtonActionKind::Unknown);
}

TEST(ButtonMappingTest, ButtonActionNumberStartsAtOne)
{
    auto spec = g703Button5();
    spec.action = ButtonAction::button(0);

    const auto mapping = ButtonMapping::create(spec);

    ASSERT_FALSE(mapping.has_value());
    EXPECT_EQ(mapping.error().code, ErrorCode::InvalidArgument);
}

TEST(ButtonMappingTest, RejectsRepeatedSupportedKind)
{
    auto spec = g703Button5();
    spec.supportedKinds = {ButtonActionKind::Button, ButtonActionKind::Button};

    EXPECT_EQ(ButtonMapping::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(ButtonMappingTest, UnknownIsNotASupportedKind)
{
    auto spec = g703Button5();
    spec.supportedKinds = {ButtonActionKind::Unknown};

    EXPECT_EQ(ButtonMapping::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(ButtonMappingTest, AcceptsUnknownActionAndEmptySupportedKinds)
{
    auto spec = g703Button5();
    spec.action = ButtonAction::unknown();
    spec.supportedKinds = {};

    EXPECT_TRUE(ButtonMapping::create(spec).has_value());
}
