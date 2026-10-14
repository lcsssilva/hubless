#include "modules/profiles/application/AutomaticProfileSwitcher.hpp"

#include <gtest/gtest.h>

using ghub::profiles::ApplicationId;
using ghub::profiles::AutomaticProfileSwitcher;
using ghub::profiles::Profile;
using ghub::profiles::ProfileLibrary;
using ghub::profiles::ProfileName;

namespace {
ProfileName nameOf(const char* text)
{
    return *ProfileName::create(text);
}
ApplicationId appOf(const char* text)
{
    return *ApplicationId::create(text);
}

// Perfis "a" (firefox) e "b" (konsole), troca ligada.
ProfileLibrary sampleLibrary(bool enabled = true)
{
    auto library =
        *ProfileLibrary::create({*Profile::create(nameOf("a"), {{1, "x", 1}}, std::nullopt, {appOf("firefox")}),
                                 *Profile::create(nameOf("b"), {{1, "x", 2}}, std::nullopt, {appOf("konsole")})});
    library.setAutomaticSwitching(enabled);
    return library;
}
} // namespace

TEST(AutomaticProfileSwitcher, StartsWithoutCurrentProfile)
{
    const AutomaticProfileSwitcher switcher;
    EXPECT_FALSE(switcher.current().has_value());
}

TEST(AutomaticProfileSwitcher, DecidesTheProfileOfTheRule)
{
    const AutomaticProfileSwitcher switcher;
    const auto decision = switcher.decide(sampleLibrary(), appOf("firefox"));
    ASSERT_TRUE(decision.has_value());
    EXPECT_EQ(decision->value(), "a");
}

TEST(AutomaticProfileSwitcher, DoesNothingWhenSwitchingIsOff)
{
    const AutomaticProfileSwitcher switcher;
    EXPECT_FALSE(switcher.decide(sampleLibrary(false), appOf("firefox")).has_value());
}

TEST(AutomaticProfileSwitcher, IgnoresTheOwnApplicationEvenWithARule)
{
    auto library = sampleLibrary();
    ASSERT_TRUE(library.assignApplication(nameOf("b"), ApplicationId::ownApplication()).has_value());
    const AutomaticProfileSwitcher switcher;
    EXPECT_FALSE(switcher.decide(library, ApplicationId::ownApplication()).has_value());
}

TEST(AutomaticProfileSwitcher, DoesNothingWithoutARule)
{
    const AutomaticProfileSwitcher switcher;
    EXPECT_FALSE(switcher.decide(sampleLibrary(), appOf("dolphin")).has_value());
}

TEST(AutomaticProfileSwitcher, DoesNotReapplyTheCurrentProfile)
{
    AutomaticProfileSwitcher switcher;
    switcher.noteApplied(nameOf("a"));
    ASSERT_TRUE(switcher.current().has_value());
    EXPECT_EQ(switcher.current()->value(), "a");
    EXPECT_FALSE(switcher.decide(sampleLibrary(), appOf("firefox")).has_value());
    const auto other = switcher.decide(sampleLibrary(), appOf("konsole"));
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(other->value(), "b");
}

TEST(AutomaticProfileSwitcher, ManualApplicationCountsAsCurrent)
{
    AutomaticProfileSwitcher switcher;
    switcher.noteApplied(nameOf("a"));
    switcher.noteApplied(nameOf("b"));
    EXPECT_EQ(switcher.current()->value(), "b");
    EXPECT_TRUE(switcher.decide(sampleLibrary(), appOf("firefox")).has_value());
    EXPECT_FALSE(switcher.decide(sampleLibrary(), appOf("konsole")).has_value());
}
