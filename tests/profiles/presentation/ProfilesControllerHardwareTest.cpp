#include "modules/profiles/presentation/ProfilesController.hpp"

#include "fakes/FakeProfileHardwarePorts.hpp"
#include "fakes/FakeProfilePorts.hpp"

#include <QSignalSpy>
#include <QTest>

#include <memory>
#include <thread>

// INC-017 parte 2 (R10/R11): seções de hardware no ProfilesController.

using ghub::profiles::ApplyProfile;
using ghub::profiles::HardwareSections;
using ghub::profiles::HeadsetLightSetting;
using ghub::profiles::KeyboardEffectSetting;
using ghub::profiles::PointerDpiSetting;
using ghub::profiles::Profile;
using ghub::profiles::ProfileBook;
using ghub::profiles::ProfileLibrary;
using ghub::profiles::ProfileName;
using ghub::profiles::ProfilesController;
using ghub::shared_kernel::ErrorCode;
using ghub::shared_kernel::fail;

namespace {

// Registra em qual thread foi chamado.
class ThreadAwareKeyboard final : public ghub::profiles::KeyboardEffectPort {
public:
    std::optional<KeyboardEffectSetting> effect;
    ghub::shared_kernel::Result<void> applyResult{};
    std::vector<KeyboardEffectSetting> applied;
    std::thread::id applyThread;

    [[nodiscard]] std::optional<KeyboardEffectSetting> last() const override { return effect; }
    [[nodiscard]] ghub::shared_kernel::Result<void> apply(const KeyboardEffectSetting& wanted) override
    {
        applyThread = std::this_thread::get_id();
        applied.push_back(wanted);
        return applyResult;
    }
};

Profile hardwareProfile(const char* name)
{
    HardwareSections hardware;
    hardware.sidetone = 35;
    hardware.headsetLights = {{.zone = 0, .effect = 3, .red = 0, .green = 0, .blue = 0, .periodMs = 5000,
                               .brightness = 100}};
    hardware.keyboardEffect = KeyboardEffectSetting{.kind = "wave", .red = 1, .green = 2, .blue = 3, .brightness = 90};
    hardware.pointerDpi =
        PointerDpiSetting{.reportRateHz = 500, .dpiStages = {400, 3200}, .defaultStage = 1, .dpiShiftStage = 0};
    return Profile::create(ProfileName::create(name).value(), {}, std::nullopt, {}, hardware).value();
}

struct Fixture {
    ghub::test::FakeProfileRepository repository;
    ghub::test::FakeCameraSettingsPort camera;
    ghub::test::FakeEqualizerSettingsPort equalizer;
    ghub::test::FakeHeadsetSettingsPort headset;
    ThreadAwareKeyboard keyboard;
    ghub::test::FakePointerSettingsPort pointer;
    ghub::profiles::HardwarePorts ports{.headset = &headset, .keyboard = &keyboard, .pointer = &pointer};
    ProfileBook book{repository, camera, nullptr, ports};
    ApplyProfile applyProfile{camera, equalizer, ports};
    ghub::test::FakeActiveApplicationSource source;
    ProfilesController controller{book, applyProfile, equalizer, source};

    void loadLibrary(ProfileLibrary library)
    {
        repository.loadResult = std::move(library);
        controller.load();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    }

    void waitIdle() { QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000); }
};

class ProfilesControllerHardwareTest : public QObject {
    Q_OBJECT

private slots:
    void init() { fixture = std::make_unique<Fixture>(); }
    void cleanup() { fixture.reset(); }

    void load_NeverTouchesHardware()
    {
        fixture->loadLibrary(ProfileLibrary::create({hardwareProfile("A")}).value());
        QVERIFY(fixture->headset.appliedSidetone.empty());
        QVERIFY(fixture->headset.appliedLights.empty());
        QVERIFY(fixture->keyboard.applied.empty());
        QVERIFY(fixture->pointer.applied.empty());
        QCOMPARE(fixture->pointer.readCalls, 0);
    }

    void apply_AppliesEverySectionAndReportsEachOne()
    {
        fixture->loadLibrary(ProfileLibrary::create({hardwareProfile("A")}).value());
        fixture->controller.apply("A");
        fixture->waitIdle();

        QCOMPARE(fixture->headset.appliedSidetone, std::vector<int>{35});
        QCOMPARE(fixture->headset.appliedLights.size(), size_t{1});
        QCOMPARE(fixture->pointer.applied.size(), size_t{1});
        QCOMPARE(fixture->keyboard.applied.size(), size_t{1});
        // O efeito do teclado é pedido na thread dona (a da janela); o resto, no worker.
        QCOMPARE(fixture->keyboard.applyThread, std::this_thread::get_id());
        const QString report = fixture->controller.report().join("\n");
        QVERIFY2(report.contains("Sidetone do G733: aplicado"), qPrintable(report));
        QVERIFY2(report.contains("Luz do G733 (inferior): aplicado"), qPrintable(report));
        QVERIFY2(report.contains("DPI do G703: aplicado"), qPrintable(report));
        QVERIFY2(report.contains("Efeito do G915: aplicado"), qPrintable(report));
        QCOMPARE(fixture->controller.status(), QStringLiteral("Perfil aplicado: A"));
    }

    void apply_OneSectionFails_OthersAreAppliedAndTheResultSaysSo()
    {
        fixture->headset.sidetoneResult = fail(ErrorCode::DeviceBusy, "em uso por headsetcontrol");
        fixture->pointer.applyResult = fail(ErrorCode::BackendUnavailable, "mouse ausente");
        fixture->loadLibrary(ProfileLibrary::create({hardwareProfile("A")}).value());
        fixture->controller.apply("A");
        fixture->waitIdle();

        const QString report = fixture->controller.report().join("\n");
        QVERIFY2(report.contains("Sidetone do G733: falhou — em uso por headsetcontrol"), qPrintable(report));
        QVERIFY2(report.contains("DPI do G703: falhou — mouse ausente"), qPrintable(report));
        QVERIFY2(report.contains("Luz do G733 (inferior): aplicado"), qPrintable(report));
        QVERIFY2(report.contains("Efeito do G915: aplicado"), qPrintable(report));
        QVERIFY2(fixture->controller.status().contains("com falhas"), qPrintable(fixture->controller.status()));
        QCOMPARE(fixture->keyboard.applied.size(), size_t{1}); // o teclado ainda foi pedido
    }

    void apply_KeyboardRefusal_IsReportedAsAFailure()
    {
        fixture->keyboard.applyResult = fail(ErrorCode::DeviceBusy, "teclado ocupado");
        fixture->loadLibrary(ProfileLibrary::create({hardwareProfile("A")}).value());
        fixture->controller.apply("A");
        fixture->waitIdle();
        QVERIFY(fixture->controller.report().join("\n").contains("Efeito do G915: falhou — teclado ocupado"));
        QVERIFY(fixture->controller.status().contains("com falhas"));
    }

    void setPersistent_AcceptsTheNewSectionsAndTheyAreNotApplied()
    {
        fixture->loadLibrary(ProfileLibrary::create({hardwareProfile("A")}).value());
        for (const char* section : {"sidetone", "headsetLights", "keyboardEffect", "pointerDpi"}) {
            fixture->controller.setPersistent(section, true);
            fixture->waitIdle();
        }
        QVERIFY(fixture->controller.persistentSidetone());
        QVERIFY(fixture->controller.persistentHeadsetLights());
        QVERIFY(fixture->controller.persistentKeyboardEffect());
        QVERIFY(fixture->controller.persistentPointerDpi());
        QVERIFY(!fixture->controller.persistentCamera());

        fixture->controller.apply("A");
        fixture->waitIdle();
        QVERIFY(fixture->headset.appliedSidetone.empty());
        QVERIFY(fixture->headset.appliedLights.empty());
        QVERIFY(fixture->pointer.applied.empty());
        QVERIFY(fixture->keyboard.applied.empty());
        const QString report = fixture->controller.report().join("\n");
        QVERIFY2(report.contains("Sidetone do G733: igual em todos os perfis"), qPrintable(report));
        QVERIFY2(report.contains("DPI do G703: igual em todos os perfis"), qPrintable(report));
    }

    void saveCurrent_CapturesHardwareAndReportsWhatWasOmitted()
    {
        fixture->headset.sidetone = 60;
        fixture->pointer.current = fail(ErrorCode::DeviceBusy, "em uso por ratbagd");
        fixture->controller.saveCurrent("Novo", false);
        fixture->waitIdle();

        QCOMPARE(fixture->repository.saveCalls, 1);
        const Profile& saved = fixture->repository.lastSaved.profiles().front();
        QCOMPARE(saved.hardware().sidetone.value_or(-1), 60);
        QVERIFY(!saved.hardware().pointerDpi.has_value());
        const QString report = fixture->controller.report().join("\n");
        QVERIFY2(report.contains("Sidetone do G733: incluído"), qPrintable(report));
        QVERIFY2(report.contains("DPI do G703 não incluído: em uso por ratbagd"), qPrintable(report));
        QVERIFY2(report.contains("Efeito do G915 não incluído"), qPrintable(report));
        QVERIFY(fixture->headset.appliedSidetone.empty()); // salvar nunca escreve
    }

    void automaticSwitching_AppliesHardwareSectionsToo()
    {
        const auto app = ghub::profiles::ApplicationId::create("firefox").value();
        HardwareSections hardware;
        hardware.sidetone = 20;
        auto profile = Profile::create(ProfileName::create("Nav").value(), {}, std::nullopt, {app}, hardware).value();
        auto library = ProfileLibrary::create({profile}).value();
        library.setAutomaticSwitching(true);
        fixture->loadLibrary(std::move(library));
        fixture->source.emitActivated("janela-inicial");
        fixture->source.emitActivated("firefox");
        QTRY_VERIFY_WITH_TIMEOUT(!fixture->headset.appliedSidetone.empty(), 5000);
        QCOMPARE(fixture->headset.appliedSidetone, std::vector<int>{20});
        fixture->waitIdle();
    }

private:
    std::unique_ptr<Fixture> fixture;
};

} // namespace

QTEST_GUILESS_MAIN(ProfilesControllerHardwareTest)
#include "ProfilesControllerHardwareTest.moc"
