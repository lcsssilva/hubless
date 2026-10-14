#include "modules/audio_processing/application/HostEqualizer.hpp"

#include "fakes/FakeAudioGraph.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

using ghub::audio_processing::EqualizerCurve;

using ghub::audio_processing::HostEqualizer;
using ghub::fakes::FakeAudioGraph;
using Op = FakeAudioGraph::Op;
using OpList = std::vector<Op>;

EqualizerCurve bassBoost()
{
    return EqualizerCurve::flat().withBand(0, 6).value();
}

TEST(HostEqualizer, Construction_DoesNotTouchGraph)
{
    FakeAudioGraph graph;
    const HostEqualizer eq{graph};
    EXPECT_TRUE(graph.calls.empty());
    EXPECT_FALSE(eq.enabled());
}

TEST(HostEqualizer, Enable_HeadsetSinkNotFound_ReturnsErrorAndCreatesNothing)
{
    FakeAudioGraph graph;
    graph.failOn(Op::FindSink, "sem G733");
    HostEqualizer eq{graph};

    const auto result = eq.enable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "sem G733");
    EXPECT_EQ(graph.ops(), OpList{Op::FindSink});
    EXPECT_FALSE(eq.enabled());
}

TEST(HostEqualizer, Enable_Success_CreatesThenReadsThenWritesPreference)
{
    FakeAudioGraph graph;
    graph.configuredDefault = "alsa_output.speakers";
    HostEqualizer eq{graph};

    ASSERT_TRUE(eq.enable().has_value());

    EXPECT_EQ(graph.ops(), (OpList{Op::FindSink, Op::Create, Op::GetDefault, Op::SetDefault}));
    EXPECT_EQ(graph.calls[1].argument, "alsa_output.headset");
    EXPECT_EQ(graph.calls[3].argument, "ghub.g733.eq");
    EXPECT_TRUE(graph.equalizerExists);
    EXPECT_EQ(graph.equalizerCurve, EqualizerCurve::flat());
    EXPECT_TRUE(eq.enabled());
}

TEST(HostEqualizer, Enable_CreateFails_ReturnsErrorWithoutPreferenceWrite)
{
    FakeAudioGraph graph;
    graph.failOn(Op::Create, "create");
    HostEqualizer eq{graph};

    const auto result = eq.enable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "create");
    EXPECT_EQ(graph.ops(), (OpList{Op::FindSink, Op::Create}));
    EXPECT_FALSE(eq.enabled());
}

TEST(HostEqualizer, Enable_ReadPreferenceFails_RemovesEqualizerAndReturnsOriginalError)
{
    FakeAudioGraph graph;
    graph.failOn(Op::GetDefault, "leitura");
    HostEqualizer eq{graph};

    const auto result = eq.enable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "leitura");
    EXPECT_EQ(graph.ops(), (OpList{Op::FindSink, Op::Create, Op::GetDefault, Op::Remove}));
    EXPECT_FALSE(graph.equalizerExists);
    EXPECT_FALSE(eq.enabled());
}

TEST(HostEqualizer, Enable_WritePreferenceFails_RemovesEqualizerAndReturnsOriginalError)
{
    FakeAudioGraph graph;
    graph.failOn(Op::SetDefault, "escrita");
    HostEqualizer eq{graph};

    const auto result = eq.enable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "escrita");
    EXPECT_EQ(graph.ops(), (OpList{Op::FindSink, Op::Create, Op::GetDefault, Op::SetDefault, Op::Remove}));
    EXPECT_FALSE(graph.equalizerExists);
    EXPECT_FALSE(eq.enabled());
}

TEST(HostEqualizer, Enable_AlreadyEnabled_SucceedsWithoutCalls)
{
    FakeAudioGraph graph;
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.enable().has_value());
    graph.calls.clear();

    EXPECT_TRUE(eq.enable().has_value());

    EXPECT_TRUE(graph.calls.empty());
}

TEST(HostEqualizer, ChangeCurve_Disabled_OnlyStoresCurve)
{
    FakeAudioGraph graph;
    HostEqualizer eq{graph};

    EXPECT_TRUE(eq.changeCurve(bassBoost()).has_value());

    EXPECT_TRUE(graph.calls.empty());
    EXPECT_EQ(eq.curve(), bassBoost());
}

TEST(HostEqualizer, ChangeCurve_Disabled_ThenEnableCreatesWithNewCurve)
{
    FakeAudioGraph graph;
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.changeCurve(bassBoost()).has_value());

    ASSERT_TRUE(eq.enable().has_value());

    EXPECT_EQ(graph.equalizerCurve, bassBoost());
}

TEST(HostEqualizer, ChangeCurve_Enabled_UpdatesEqualizer)
{
    FakeAudioGraph graph;
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.enable().has_value());
    graph.calls.clear();

    EXPECT_TRUE(eq.changeCurve(bassBoost()).has_value());

    EXPECT_EQ(graph.ops(), OpList{Op::Update});
    EXPECT_EQ(graph.equalizerCurve, bassBoost());
    EXPECT_EQ(eq.curve(), bassBoost());
}

TEST(HostEqualizer, ChangeCurve_UpdateFails_ReturnsErrorAndKeepsPreviousCurve)
{
    FakeAudioGraph graph;
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.enable().has_value());
    graph.failOn(Op::Update, "update");

    const auto result = eq.changeCurve(bassBoost());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "update");
    EXPECT_EQ(eq.curve(), EqualizerCurve::flat());
    EXPECT_TRUE(eq.enabled());
}

TEST(HostEqualizer, Disable_Disabled_SucceedsWithoutCalls)
{
    FakeAudioGraph graph;
    HostEqualizer eq{graph};

    EXPECT_TRUE(eq.disable().has_value());

    EXPECT_TRUE(graph.calls.empty());
}

TEST(HostEqualizer, Disable_PreferenceStillEqualizer_RestoresSavedThenRemoves)
{
    FakeAudioGraph graph;
    graph.configuredDefault = "alsa_output.speakers";
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.enable().has_value());
    graph.calls.clear();

    EXPECT_TRUE(eq.disable().has_value());

    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::SetDefault, Op::Remove}));
    EXPECT_EQ(graph.calls[1].argument, "alsa_output.speakers");
    EXPECT_EQ(graph.configuredDefault, "alsa_output.speakers");
    EXPECT_FALSE(graph.equalizerExists);
    EXPECT_FALSE(eq.enabled());
}

TEST(HostEqualizer, Disable_SavedPreferenceWasAbsent_ErasesKey)
{
    FakeAudioGraph graph;
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.enable().has_value());
    graph.calls.clear();

    EXPECT_TRUE(eq.disable().has_value());

    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::SetDefault, Op::Remove}));
    EXPECT_EQ(graph.calls[1].argument, std::nullopt);
    EXPECT_EQ(graph.configuredDefault, std::nullopt);
}

TEST(HostEqualizer, Disable_UserChangedPreference_DoesNotOverwriteIt)
{
    FakeAudioGraph graph;
    graph.configuredDefault = "alsa_output.speakers";
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.enable().has_value());
    graph.configuredDefault = "alsa_output.other";
    graph.calls.clear();

    EXPECT_TRUE(eq.disable().has_value());

    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::Remove}));
    EXPECT_EQ(graph.configuredDefault, "alsa_output.other");
    EXPECT_FALSE(eq.enabled());
}

TEST(HostEqualizer, Disable_ReadPreferenceFails_StillRemovesAndReturnsFirstError)
{
    FakeAudioGraph graph;
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.enable().has_value());
    graph.failOn(Op::GetDefault, "leitura");
    graph.failOn(Op::Remove, "remove");
    graph.calls.clear();

    const auto result = eq.disable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "leitura");
    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::Remove}));
    EXPECT_FALSE(eq.enabled());
}

TEST(HostEqualizer, Disable_RestoreFails_StillRemovesAndReturnsRestoreError)
{
    FakeAudioGraph graph;
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.enable().has_value());
    graph.failOn(Op::SetDefault, "restaura");
    graph.calls.clear();

    const auto result = eq.disable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "restaura");
    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::SetDefault, Op::Remove}));
    EXPECT_FALSE(eq.enabled());
}

TEST(HostEqualizer, Disable_RemoveFails_ReturnsErrorAndEndsDisabled)
{
    FakeAudioGraph graph;
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.enable().has_value());
    graph.failOn(Op::Remove, "remove");

    const auto result = eq.disable();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "remove");
    EXPECT_FALSE(eq.enabled());
}

TEST(HostEqualizer, Enable_AfterDisable_SavesCurrentPreferenceAgain)
{
    FakeAudioGraph graph;
    graph.configuredDefault = "alsa_output.speakers";
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.enable().has_value());
    ASSERT_TRUE(eq.disable().has_value());
    graph.configuredDefault = "alsa_output.other";
    ASSERT_TRUE(eq.enable().has_value());

    ASSERT_TRUE(eq.disable().has_value());

    EXPECT_EQ(graph.configuredDefault, "alsa_output.other");
}

TEST(HostEqualizer, EqualizerLost_Enabled_RestoresPreferenceRemovesAndDisables)
{
    FakeAudioGraph graph;
    graph.configuredDefault = "alsa_output.speakers";
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.enable().has_value());
    graph.calls.clear();

    eq.equalizerLost();

    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::SetDefault, Op::Remove}));
    EXPECT_EQ(graph.configuredDefault, "alsa_output.speakers");
    EXPECT_FALSE(eq.enabled());
}

TEST(HostEqualizer, EqualizerLost_UserChangedPreference_DoesNotOverwriteIt)
{
    FakeAudioGraph graph;
    HostEqualizer eq{graph};
    ASSERT_TRUE(eq.enable().has_value());
    graph.configuredDefault = "alsa_output.other";
    graph.calls.clear();

    eq.equalizerLost();

    EXPECT_EQ(graph.ops(), (OpList{Op::GetDefault, Op::Remove}));
    EXPECT_EQ(graph.configuredDefault, "alsa_output.other");
    EXPECT_FALSE(eq.enabled());
}

TEST(HostEqualizer, EqualizerLost_Disabled_DoesNothing)
{
    FakeAudioGraph graph;
    HostEqualizer eq{graph};

    eq.equalizerLost();

    EXPECT_TRUE(graph.calls.empty());
    EXPECT_FALSE(eq.enabled());
}

} // namespace
