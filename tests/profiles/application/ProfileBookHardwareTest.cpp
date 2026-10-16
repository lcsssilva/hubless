#include "modules/profiles/application/ProfileBook.hpp"
#include "fakes/FakeProfileHardwarePorts.hpp"
#include "fakes/FakeProfilePorts.hpp"

#include <gtest/gtest.h>

// INC-017 parte 2 (R10): "Salvar estado atual" captura as seções de hardware que a janela conhece.

using ghub::profiles::HardwareSections;
using ghub::profiles::HeadsetLightSetting;
using ghub::profiles::ImportedProfile;
using ghub::profiles::KeyboardEffectSetting;
using ghub::profiles::PointerDpiSetting;
using ghub::profiles::Profile;
using ghub::profiles::ProfileBook;
using ghub::profiles::ProfileName;
using ghub::profiles::ProfileSection;
using ghub::shared_kernel::ErrorCode;
using ghub::test::FakeCameraSettingsPort;
using ghub::test::FakeHeadsetSettingsPort;
using ghub::test::FakeKeyboardEffectPort;
using ghub::test::FakePointerSettingsPort;
using ghub::test::FakeProfileRepository;
using ghub::test::FakeProfileTransfer;

namespace {

ProfileName nameOf(const char* text)
{
    return *ProfileName::create(text);
}

const ProfileBook::SectionNote* noteFor(const ProfileBook::SaveReport& report, ProfileSection section)
{
    for (const auto& note : report.hardware) {
        if (note.section == section) {
            return &note;
        }
    }
    return nullptr;
}

struct Fixture {
    FakeProfileRepository repository;
    FakeCameraSettingsPort camera;
    FakeProfileTransfer transfer;
    FakeHeadsetSettingsPort headset;
    FakeKeyboardEffectPort keyboard;
    FakePointerSettingsPort pointer;
    ProfileBook book{repository, camera, &transfer, {.headset = &headset, .keyboard = &keyboard, .pointer = &pointer}};

    Fixture()
    {
        camera.current = ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "sem câmera");
        headset.sidetone = 40;
        headset.lights = {{.zone = 1, .effect = 2, .red = 9, .green = 8, .blue = 7, .periodMs = 3000, .brightness = 50}};
        keyboard.effect = KeyboardEffectSetting{.kind = "ripple", .red = 4, .green = 5, .blue = 6, .brightness = 100};
        EXPECT_TRUE(book.load().has_value());
    }
};

} // namespace

TEST(ProfileBookHardware, SaveCurrentCapturesWhatEachOwnerKnows)
{
    Fixture f;
    const auto report = f.book.saveCurrent("Jogo", std::nullopt, false);
    ASSERT_TRUE(report.has_value());
    const Profile* saved = f.book.library().find(nameOf("Jogo"));
    ASSERT_NE(saved, nullptr);
    EXPECT_EQ(saved->hardware().sidetone, 40);
    ASSERT_EQ(saved->hardware().headsetLights.size(), 1U);
    EXPECT_EQ(saved->hardware().headsetLights[0].red, 9);
    ASSERT_TRUE(saved->hardware().keyboardEffect.has_value());
    EXPECT_EQ(saved->hardware().keyboardEffect->kind, "ripple");
    ASSERT_TRUE(saved->hardware().pointerDpi.has_value());
    EXPECT_EQ(saved->hardware().pointerDpi->dpiStages, (std::vector<int>{800, 1600}));
    for (const auto section : {ProfileSection::Sidetone, ProfileSection::HeadsetLights, ProfileSection::KeyboardEffect,
                               ProfileSection::PointerDpi}) {
        const auto* note = noteFor(*report, section);
        ASSERT_NE(note, nullptr);
        EXPECT_TRUE(note->included);
    }
    EXPECT_EQ(f.pointer.readCalls, 1);
    EXPECT_TRUE(f.pointer.applied.empty()); // salvar nunca escreve
    EXPECT_TRUE(f.headset.appliedSidetone.empty());
}

TEST(ProfileBookHardware, SectionsWithoutAKnownValueAreOmittedWithAReason)
{
    Fixture f;
    f.headset.sidetone.reset();
    f.headset.lights.clear();
    f.keyboard.effect.reset();
    f.pointer.current = ghub::shared_kernel::fail(ErrorCode::DeviceBusy, "em uso por ratbagd");
    f.camera.current = std::vector<ghub::profiles::CameraControlSetting>{{1, "Brilho", 10}};
    const auto report = f.book.saveCurrent("a", std::nullopt, false);
    ASSERT_TRUE(report.has_value());
    EXPECT_TRUE(f.book.library().find(nameOf("a"))->hardware().empty());
    for (const auto section : {ProfileSection::Sidetone, ProfileSection::HeadsetLights, ProfileSection::KeyboardEffect,
                               ProfileSection::PointerDpi}) {
        const auto* note = noteFor(*report, section);
        ASSERT_NE(note, nullptr);
        EXPECT_FALSE(note->included);
        EXPECT_FALSE(note->omittedReason.empty());
    }
    EXPECT_NE(noteFor(*report, ProfileSection::PointerDpi)->omittedReason.find("ratbagd"), std::string::npos);
}

TEST(ProfileBookHardware, AHardwareOnlyProfileIsValid)
{
    Fixture f;
    f.keyboard.effect.reset();
    f.headset.lights.clear();
    f.pointer.current = ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "sem mouse");
    ASSERT_TRUE(f.book.saveCurrent("so-sidetone", std::nullopt, false).has_value());
    EXPECT_EQ(f.book.library().find(nameOf("so-sidetone"))->hardware().sidetone, 40);
}

TEST(ProfileBookHardware, WithoutAnyHardwarePortsTheOldBehaviorHolds)
{
    FakeProfileRepository repository;
    FakeCameraSettingsPort camera;
    camera.current = std::vector<ghub::profiles::CameraControlSetting>{{1, "Brilho", 10}};
    ProfileBook book{repository, camera};
    ASSERT_TRUE(book.load().has_value());
    const auto report = book.saveCurrent("a", std::nullopt, false);
    ASSERT_TRUE(report.has_value());
    EXPECT_TRUE(book.library().find(nameOf("a"))->hardware().empty());
    const auto* note = noteFor(*report, ProfileSection::Sidetone);
    ASSERT_NE(note, nullptr);
    EXPECT_FALSE(note->included);
}

TEST(ProfileBookHardware, PersistentSectionIsPropagatedOnSave)
{
    Fixture f;
    ASSERT_TRUE(f.book.saveCurrent("a", std::nullopt, false).has_value());
    ASSERT_TRUE(f.book.saveCurrent("b", std::nullopt, false).has_value());
    ASSERT_TRUE(f.book.setPersistent(ProfileSection::Sidetone, true).has_value());
    f.headset.sidetone = 75;
    ASSERT_TRUE(f.book.saveCurrent("a", std::nullopt, true).has_value());
    EXPECT_EQ(f.book.library().find(nameOf("b"))->hardware().sidetone, 75);
    EXPECT_EQ(f.book.library().find(nameOf("a"))->hardware().sidetone, 75);
}

TEST(ProfileBookHardware, DuplicateExportAndImportKeepHardwareSections)
{
    Fixture f;
    ASSERT_TRUE(f.book.saveCurrent("a", std::nullopt, false).has_value());
    ASSERT_TRUE(f.book.duplicate("a", "b").has_value());
    EXPECT_EQ(f.book.library().find(nameOf("b"))->hardware().sidetone, 40);

    ASSERT_TRUE(f.book.exportProfile("a", "/tmp/x.json").has_value());
    ASSERT_EQ(f.transfer.exported.size(), 1U);
    EXPECT_EQ(f.transfer.exported[0].hardware().sidetone, 40);

    HardwareSections hardware;
    hardware.sidetone = 12;
    f.transfer.importResult = ImportedProfile{*Profile::create(nameOf("c"), {}, std::nullopt, {}, hardware), {}};
    ASSERT_TRUE(f.book.importProfile("/tmp/c.json", false).has_value());
    EXPECT_EQ(f.book.library().find(nameOf("c"))->hardware().sidetone, 12);
}
