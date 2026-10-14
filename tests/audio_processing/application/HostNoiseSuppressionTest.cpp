#include "modules/audio_processing/application/HostNoiseSuppression.hpp"

#include "fakes/FakeNoiseSuppressionGraph.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

using ghub::audio_processing::HostNoiseSuppression;
using ghub::audio_processing::VoiceThreshold;
using ghub::fakes::FakeNoiseSuppressionGraph;
using Op = FakeNoiseSuppressionGraph::Op;
using OpList = std::vector<Op>;

VoiceThreshold strict()
{
    return VoiceThreshold::create(80).value();
}

TEST(HostNoiseSuppression, Construction_DoesNotTouchGraph)
{
    FakeNoiseSuppressionGraph graph;
    const HostNoiseSuppression ns{graph};
    EXPECT_TRUE(graph.calls.empty());
    EXPECT_FALSE(ns.enabled());
    EXPECT_EQ(ns.threshold(), VoiceThreshold::standard());
}

TEST(HostNoiseSuppression, Enable_HeadsetSourceNotFound_ReturnsErrorAndCreatesNothing)
{
    FakeNoiseSuppressionGraph graph;
    graph.failOn(Op::FindSource, "sem G733");
    HostNoiseSuppression ns{graph};

    const auto result = ns.enable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "sem G733");
    EXPECT_EQ(graph.ops(), OpList{Op::FindSource});
    EXPECT_FALSE(ns.enabled());
}

TEST(HostNoiseSuppression, Enable_Success_CreatesThenReadsThenWritesPreference)
{
    FakeNoiseSuppressionGraph graph;
    graph.configuredDefault = "alsa_input.webcam";
    HostNoiseSuppression ns{graph};

    ASSERT_TRUE(ns.enable().has_value());

    EXPECT_EQ(graph.ops(), (OpList{Op::FindSource, Op::Create, Op::GetDefault, Op::SetDefault}));
    EXPECT_EQ(graph.calls[1].argument, "alsa_input.headset");
    EXPECT_EQ(graph.calls[3].argument, "ghub.g733.mic");
    EXPECT_TRUE(graph.suppressionExists);
    EXPECT_EQ(graph.suppressionThreshold, VoiceThreshold::standard());
    EXPECT_TRUE(ns.enabled());
}

TEST(HostNoiseSuppression, Enable_CreateFails_ReturnsErrorWithoutPreferenceWrite)
{
    FakeNoiseSuppressionGraph graph;
    graph.failOn(Op::Create, "create");
    HostNoiseSuppression ns{graph};

    const auto result = ns.enable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "create");
    EXPECT_EQ(graph.ops(), (OpList{Op::FindSource, Op::Create}));
    EXPECT_FALSE(ns.enabled());
}

TEST(HostNoiseSuppression, Enable_ReadPreferenceFails_RemovesSourceAndReturnsOriginalError)
{
    FakeNoiseSuppressionGraph graph;
    graph.failOn(Op::GetDefault, "leitura");
    HostNoiseSuppression ns{graph};

    const auto result = ns.enable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "leitura");
    EXPECT_EQ(graph.ops(), (OpList{Op::FindSource, Op::Create, Op::GetDefault, Op::Remove}));
    EXPECT_FALSE(graph.suppressionExists);
    EXPECT_FALSE(ns.enabled());
}

TEST(HostNoiseSuppression, Enable_WritePreferenceFails_RemovesSourceAndReturnsOriginalError)
{
    FakeNoiseSuppressionGraph graph;
    graph.failOn(Op::SetDefault, "escrita");
    HostNoiseSuppression ns{graph};

    const auto result = ns.enable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "escrita");
    EXPECT_EQ(graph.ops(), (OpList{Op::FindSource, Op::Create, Op::GetDefault, Op::SetDefault, Op::Remove}));
    EXPECT_FALSE(graph.suppressionExists);
    EXPECT_FALSE(ns.enabled());
}

TEST(HostNoiseSuppression, Enable_AlreadyEnabled_SucceedsWithoutCalls)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    graph.calls.clear();

    EXPECT_TRUE(ns.enable().has_value());

    EXPECT_TRUE(graph.calls.empty());
}

TEST(HostNoiseSuppression, ChangeThreshold_Disabled_OnlyStoresThreshold)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};

    EXPECT_TRUE(ns.changeThreshold(strict()).has_value());

    EXPECT_TRUE(graph.calls.empty());
    EXPECT_EQ(ns.threshold(), strict());
}

TEST(HostNoiseSuppression, ChangeThreshold_Disabled_ThenEnableCreatesWithNewThreshold)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.changeThreshold(strict()).has_value());

    ASSERT_TRUE(ns.enable().has_value());

    EXPECT_EQ(graph.suppressionThreshold, strict());
}

TEST(HostNoiseSuppression, ChangeThreshold_Enabled_UpdatesSource)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    graph.calls.clear();

    EXPECT_TRUE(ns.changeThreshold(strict()).has_value());

    EXPECT_EQ(graph.ops(), OpList{Op::Update});
    EXPECT_EQ(graph.suppressionThreshold, strict());
    EXPECT_EQ(ns.threshold(), strict());
}

TEST(HostNoiseSuppression, ChangeThreshold_UpdateFails_ReturnsErrorAndKeepsPreviousThreshold)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    graph.failOn(Op::Update, "update");

    const auto result = ns.changeThreshold(strict());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "update");
    EXPECT_EQ(ns.threshold(), VoiceThreshold::standard());
    EXPECT_TRUE(ns.enabled());
}

TEST(HostNoiseSuppression, Disable_Disabled_SucceedsWithoutCalls)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};

    EXPECT_TRUE(ns.disable().has_value());

    EXPECT_TRUE(graph.calls.empty());
}

TEST(HostNoiseSuppression, Disable_PreferenceStillOurs_RestoresSavedThenRemoves)
{
    FakeNoiseSuppressionGraph graph;
    graph.configuredDefault = "alsa_input.webcam";
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    graph.calls.clear();

    EXPECT_TRUE(ns.disable().has_value());

    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::SetDefault, Op::Remove}));
    EXPECT_EQ(graph.calls[1].argument, "alsa_input.webcam");
    EXPECT_EQ(graph.configuredDefault, "alsa_input.webcam");
    EXPECT_FALSE(graph.suppressionExists);
    EXPECT_FALSE(ns.enabled());
}

TEST(HostNoiseSuppression, Disable_SavedPreferenceWasAbsent_ErasesKey)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    graph.calls.clear();

    EXPECT_TRUE(ns.disable().has_value());

    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::SetDefault, Op::Remove}));
    EXPECT_EQ(graph.calls[1].argument, std::nullopt);
    EXPECT_EQ(graph.configuredDefault, std::nullopt);
}

TEST(HostNoiseSuppression, Disable_UserChangedPreference_DoesNotOverwriteIt)
{
    FakeNoiseSuppressionGraph graph;
    graph.configuredDefault = "alsa_input.webcam";
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    graph.configuredDefault = "alsa_input.other";
    graph.calls.clear();

    EXPECT_TRUE(ns.disable().has_value());

    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::Remove}));
    EXPECT_EQ(graph.configuredDefault, "alsa_input.other");
    EXPECT_FALSE(ns.enabled());
}

TEST(HostNoiseSuppression, Disable_ReadPreferenceFails_StillRemovesAndReturnsFirstError)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    graph.failOn(Op::GetDefault, "leitura");
    graph.failOn(Op::Remove, "remove");
    graph.calls.clear();

    const auto result = ns.disable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "leitura");
    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::Remove}));
    EXPECT_FALSE(ns.enabled());
}

TEST(HostNoiseSuppression, Disable_RestoreFails_StillRemovesAndReturnsRestoreError)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    graph.failOn(Op::SetDefault, "restaura");
    graph.calls.clear();

    const auto result = ns.disable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "restaura");
    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::SetDefault, Op::Remove}));
    EXPECT_FALSE(ns.enabled());
}

TEST(HostNoiseSuppression, Disable_RemoveFails_ReturnsErrorAndEndsDisabled)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    graph.failOn(Op::Remove, "remove");

    const auto result = ns.disable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "remove");
    EXPECT_FALSE(ns.enabled());
}

TEST(HostNoiseSuppression, Enable_AfterDisable_SavesCurrentPreferenceAgain)
{
    FakeNoiseSuppressionGraph graph;
    graph.configuredDefault = "alsa_input.webcam";
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    ASSERT_TRUE(ns.disable().has_value());
    graph.configuredDefault = "alsa_input.other";
    ASSERT_TRUE(ns.enable().has_value());

    ASSERT_TRUE(ns.disable().has_value());

    EXPECT_EQ(graph.configuredDefault, "alsa_input.other");
}

TEST(HostNoiseSuppression, ChangeThreshold_AfterDisable_EnableUsesLatestThreshold)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    ASSERT_TRUE(ns.changeThreshold(strict()).has_value());
    ASSERT_TRUE(ns.disable().has_value());

    ASSERT_TRUE(ns.enable().has_value());

    EXPECT_EQ(graph.suppressionThreshold, strict());
}

TEST(HostNoiseSuppression, NoiseSuppressionLost_Enabled_RestoresPreferenceRemovesAndDisables)
{
    FakeNoiseSuppressionGraph graph;
    graph.configuredDefault = "alsa_input.webcam";
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    graph.calls.clear();

    ns.noiseSuppressionLost();

    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::SetDefault, Op::Remove}));
    EXPECT_EQ(graph.configuredDefault, "alsa_input.webcam");
    EXPECT_FALSE(ns.enabled());
}

TEST(HostNoiseSuppression, NoiseSuppressionLost_UserChangedPreference_DoesNotOverwriteIt)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};
    ASSERT_TRUE(ns.enable().has_value());
    graph.configuredDefault = "alsa_input.other";
    graph.calls.clear();

    ns.noiseSuppressionLost();

    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::Remove}));
    EXPECT_EQ(graph.configuredDefault, "alsa_input.other");
    EXPECT_FALSE(ns.enabled());
}

TEST(HostNoiseSuppression, NoiseSuppressionLost_Disabled_DoesNothing)
{
    FakeNoiseSuppressionGraph graph;
    HostNoiseSuppression ns{graph};

    ns.noiseSuppressionLost();

    EXPECT_TRUE(graph.calls.empty());
    EXPECT_FALSE(ns.enabled());
}

} // namespace
