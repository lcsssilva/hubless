#include "modules/lighting/presentation/KeyboardControlsController.hpp"

#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeKeyboardControl.hpp"

#include <QSignalSpy>
#include <QStringList>
#include <QTest>
#include <QVariantList>

#include <memory>
#include <string>
#include <vector>

using ghub::device_access::AccessRegistry;
using ghub::device_access::ExternalOwner;
using ghub::device_access::PhysicalInterfaceId;
using ghub::lighting::application::ControlKeyboard;
using ghub::lighting::domain::OnboardSlot;
using ghub::lighting::domain::OnboardSlots;
using ghub::lighting::presentation::KeyboardControlsController;
using ghub::shared_kernel::DomainError;
using ghub::shared_kernel::ErrorCode;
using Calls = std::vector<std::string>;

namespace {

PhysicalInterfaceId g915Interface()
{
    return PhysicalInterfaceId::create("hidpp:046d:407c").value(); // NOLINT(bugprone-unchecked-optional-access)
}

OnboardSlot slot(std::uint8_t number)
{
    return *OnboardSlot::create(number); // NOLINT(bugprone-unchecked-optional-access)
}

class KeyboardControlsControllerTest : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        env = std::make_unique<Env>();
        env->probe.setResult(std::vector<ExternalOwner>{});
        env->control.setSlots(OnboardSlots{.enabled = {slot(1), slot(3)}, .active = slot(1)});
    }

    void cleanup() { env.reset(); }

    void construction_TouchesNothing_AndOffersTheSuggestedKeys()
    {
        KeyboardControlsController controller{env->useCase};

        QVERIFY(env->control.calls().empty());
        QCOMPARE(env->probe.calls(), 0);
        QVERIFY(!controller.busy());
        QCOMPARE(controller.gameModeKeys(), (QStringList{QStringLiteral("Left Windows"),
                                                         QStringLiteral("Right Windows"), QStringLiteral("Menu")}));
        QVERIFY(!controller.gameModeChoices().contains(QStringLiteral("Menu")));
        QVERIFY(controller.gameModeChoices().contains(QStringLiteral("F1")));
        QVERIFY(controller.enabledSlots().isEmpty());
        QCOMPARE(controller.activeSlot(), 0);
    }

    void activateGameMode_SendsTheSelectedKeysOnlyOnTheCall()
    {
        KeyboardControlsController controller{env->useCase};

        controller.activateGameMode();
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(env->control.calls(), (Calls{"open", "game:e3e765"}));
        QVERIFY(controller.status().contains(QStringLiteral("ativado")));
        QVERIFY(controller.status().contains(QStringLiteral("volátil")));
    }

    void addAndRemoveKeys_ChangeOnlyTheSelection_WithoutWriting()
    {
        KeyboardControlsController controller{env->useCase};

        controller.removeGameKey(QStringLiteral("Left Windows"));
        controller.removeGameKey(QStringLiteral("Right Windows"));
        controller.removeGameKey(QStringLiteral("Menu"));
        controller.addGameKey(QStringLiteral("F1"));

        QCOMPARE(controller.gameModeKeys(), (QStringList{QStringLiteral("F1")}));
        QVERIFY(env->control.calls().empty());
        controller.activateGameMode();
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(env->control.calls(), (Calls{"open", "game:3a"})); // C-04: F1 = 0x3a
    }

    void addGameKey_UnknownOrDuplicate_IsIgnored()
    {
        KeyboardControlsController controller{env->useCase};

        controller.addGameKey(QStringLiteral("Tecla que não existe"));
        controller.addGameKey(QStringLiteral("Menu"));

        QCOMPARE(controller.gameModeKeys().size(), 3);
    }

    void addGameKey_BeyondSixteen_IsRefusedWithAMessage()
    {
        KeyboardControlsController controller{env->useCase};
        for (const char* key : {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M"}) {
            controller.addGameKey(QString::fromLatin1(key));
        }
        QCOMPARE(controller.gameModeKeys().size(), 16);

        controller.addGameKey(QStringLiteral("N"));

        QCOMPARE(controller.gameModeKeys().size(), 16);
        QVERIFY(controller.status().contains(QStringLiteral("16")));
    }

    void resetGameKeys_RestoresTheSuggestedOnes()
    {
        KeyboardControlsController controller{env->useCase};
        controller.removeGameKey(QStringLiteral("Menu"));

        controller.resetGameKeys();

        QCOMPARE(controller.gameModeKeys().size(), 3);
    }

    void deactivateGameMode_SendsAnEmptyList()
    {
        KeyboardControlsController controller{env->useCase};

        controller.deactivateGameMode();
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(env->control.calls(), (Calls{"open", "game:"}));
        QVERIFY(controller.status().contains(QStringLiteral("desativado")));
    }

    void externalProcess_MakesItBusy_NamingTheProcess()
    {
        env->probe.setResult(std::vector<ExternalOwner>{ExternalOwner::OpenRgb});
        KeyboardControlsController controller{env->useCase};

        controller.activateGameMode();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(env->control.calls().empty());
        QVERIFY(controller.status().contains(QStringLiteral("openrgb")));
    }

    void failure_IsReportedAsUnavailable()
    {
        env->control.failGameMode(DomainError{.code = ErrorCode::BackendUnavailable, .detail = "erro 5"});
        KeyboardControlsController controller{env->useCase};

        controller.activateGameMode();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(controller.status().contains(QStringLiteral("indisponível")));
    }

    void refreshSlots_ListsTheEnabledOnesAndTheActive()
    {
        KeyboardControlsController controller{env->useCase};

        controller.refreshSlots();
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(controller.enabledSlots(), (QVariantList{1, 3}));
        QCOMPARE(controller.activeSlot(), 1);
        QCOMPARE(env->control.calls(), (Calls{"open", "slots"}));
    }

    void selectSlot_Enabled_SwitchesAndMarksItActive()
    {
        KeyboardControlsController controller{env->useCase};

        controller.selectSlot(3);
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(env->control.calls(), (Calls{"open", "slots", "select:3"}));
        QCOMPARE(controller.activeSlot(), 3);
    }

    void selectSlot_NotEnabled_SendsNoSwitch()
    {
        KeyboardControlsController controller{env->useCase};

        controller.selectSlot(2);
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(env->control.calls(), (Calls{"open", "slots"}));
        QCOMPARE(controller.activeSlot(), 0);
        QVERIFY(controller.status().contains(QStringLiteral("habilitado")));
    }

    void selectSlot_OutOfRange_DoesNotEvenOpen()
    {
        KeyboardControlsController controller{env->useCase};

        controller.selectSlot(7);

        QVERIFY(!controller.busy());
        QVERIFY(env->control.calls().empty());
    }

    void requestWhileBusy_IsIgnored()
    {
        KeyboardControlsController controller{env->useCase};

        controller.activateGameMode();
        controller.deactivateGameMode(); // chega com a primeira em andamento
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(env->control.calls(), (Calls{"open", "game:e3e765"}));
    }

private: // NOLINT(readability-redundant-access-specifiers)
    struct Env {
        ghub::fakes::FakeClock clock;
        AccessRegistry registry{clock};
        ghub::fakes::FakeExternalProcessProbe probe;
        ghub::fakes::FakeKeyboardControl control;
        ControlKeyboard useCase{control, registry, probe, g915Interface()};
    };
    std::unique_ptr<Env> env;
};

} // namespace

QTEST_GUILESS_MAIN(KeyboardControlsControllerTest)
#include "KeyboardControlsControllerTest.moc"
