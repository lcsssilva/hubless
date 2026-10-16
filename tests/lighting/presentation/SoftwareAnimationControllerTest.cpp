#include "modules/lighting/presentation/SoftwareAnimationController.hpp"

#include "fakes/FakeAudioLevelSource.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeKeyboardLighting.hpp"
#include "modules/lighting/domain/G915KeyCatalog.hpp"

#include <QColor>
#include <QTest>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using ghub::device_access::AccessRegistry;
using ghub::device_access::ExternalOwner;
using ghub::device_access::LeaseHolder;
using ghub::device_access::PhysicalInterfaceId;
using ghub::lighting::application::StartLiveKeyboardSession;
using ghub::lighting::domain::G915KeyCatalog;
using ghub::lighting::presentation::SoftwareAnimationController;
using ghub::shared_kernel::ErrorCode;
using namespace std::chrono_literals;

namespace {

PhysicalInterfaceId g915Interface()
{
    return PhysicalInterfaceId::create("hidpp:046d:407c").value(); // NOLINT(bugprone-unchecked-optional-access)
}

int countOf(const std::vector<std::string>& calls, const std::string& name)
{
    return static_cast<int>(std::ranges::count(calls, name));
}

} // namespace

class SoftwareAnimationControllerTest : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        rig = std::make_unique<Rig>();
        rig->probe.setResult(std::vector<ExternalOwner>{});
    }

    void cleanup() { rig.reset(); }

    void constructionOpensNothing()
    {
        auto& controller = rig->controller;
        auto& keyboard = rig->keyboard;
        auto& audio = rig->audio;
        auto& clock = rig->clock;
        auto& registry = rig->registry;
        (void)keyboard;
        (void)audio;
        (void)clock;
        (void)registry;
        (void)controller;
        QVERIFY(!controller->running());
        QVERIFY(keyboard.calls().empty());
        QCOMPARE(audio.starts(), 0);
    }

    void startingACycleOpensTheSessionSendsFramesAndStopReturnsControl()
    {
        auto& controller = rig->controller;
        auto& keyboard = rig->keyboard;
        auto& audio = rig->audio;
        auto& clock = rig->clock;
        auto& registry = rig->registry;
        (void)keyboard;
        (void)audio;
        (void)clock;
        (void)registry;
        (void)controller;
        controller->start("cycle", Qt::red, Qt::blue, 6, false, 50, 20);
        QTRY_VERIFY_WITH_TIMEOUT(controller->running(), 5000);
        QCOMPARE(controller->framesPerSecond(), 20);

        // O relógio injetado é do teste: cada avanço libera o próximo quadro do limitador de taxa.
        QTRY_VERIFY_WITH_TIMEOUT((clock.advance(100ms), controller->framesSent() >= 2), 5000);
        controller->stop();
        QTRY_VERIFY_WITH_TIMEOUT(!controller->running() && !controller->busy(), 5000);

        const auto& calls = keyboard.calls();
        QVERIFY(calls.size() >= 4);
        QCOMPARE(calls[0], std::string{"open"});
        QCOMPARE(calls[1], std::string{"begin"});
        QCOMPARE(calls[2], std::string{"paint"});
        QCOMPARE(calls.back(), std::string{"end"});
        QCOMPARE(countOf(calls, "end"), 1);
        QCOMPARE(keyboard.painted().front().size(), G915KeyCatalog::keys().size());
        QVERIFY(!audio.running());
        QCOMPARE(audio.starts(), 0); // só o visualizador abre a captura de áudio
    }

    void unknownKindOpensNothing()
    {
        auto& controller = rig->controller;
        auto& keyboard = rig->keyboard;
        auto& audio = rig->audio;
        auto& clock = rig->clock;
        auto& registry = rig->registry;
        (void)keyboard;
        (void)audio;
        (void)clock;
        (void)registry;
        (void)controller;
        controller->start("nao-existe", Qt::red, Qt::blue, 6, false, 50, 10);

        QVERIFY(!controller->running());
        QVERIFY(!controller->status().isEmpty());
        QVERIFY(keyboard.calls().empty());
    }

    void busyKeyboardIsReportedAndNothingIsOpened()
    {
        auto& controller = rig->controller;
        auto& keyboard = rig->keyboard;
        auto& audio = rig->audio;
        auto& clock = rig->clock;
        auto& registry = rig->registry;
        (void)keyboard;
        (void)audio;
        (void)clock;
        (void)registry;
        (void)controller;
        auto other = registry.acquire({.interfaceId = g915Interface(),
                                       .holder = LeaseHolder::create("outro").value(), // NOLINT
                                       .purpose = "teste",
                                       .maxDuration = 60s});
        QVERIFY(other.has_value());

        controller->start("wave", Qt::red, Qt::blue, 6, false, 50, 10);

        QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), 5000);
        QVERIFY(!controller->running());
        QVERIFY(!controller->status().isEmpty());
        QVERIFY(keyboard.calls().empty());
    }

    void audioVisualizerCapturesOnlyWhileRunning()
    {
        auto& controller = rig->controller;
        auto& keyboard = rig->keyboard;
        auto& audio = rig->audio;
        auto& clock = rig->clock;
        auto& registry = rig->registry;
        (void)keyboard;
        (void)audio;
        (void)clock;
        (void)registry;
        (void)controller;
        QCOMPARE(audio.starts(), 0);

        controller->start("audio", Qt::green, Qt::blue, 6, false, 50, 10);
        QTRY_VERIFY_WITH_TIMEOUT(controller->running(), 5000);
        QVERIFY(audio.running());

        controller->stop();
        QTRY_VERIFY_WITH_TIMEOUT(!controller->running() && !controller->busy(), 5000);
        QVERIFY(!audio.running());
        QCOMPARE(countOf(keyboard.calls(), "end"), 1);
    }

    void audioStartFailureOpensNoKeyboardSession()
    {
        auto& controller = rig->controller;
        auto& keyboard = rig->keyboard;
        auto& audio = rig->audio;
        auto& clock = rig->clock;
        auto& registry = rig->registry;
        (void)keyboard;
        (void)audio;
        (void)clock;
        (void)registry;
        (void)controller;
        audio.failStart({ErrorCode::BackendUnavailable, "sem PipeWire"});

        controller->start("audio", Qt::green, Qt::blue, 6, false, 50, 10);

        QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), 5000);
        QVERIFY(!controller->running());
        QVERIFY(!controller->status().isEmpty());
        QVERIFY(keyboard.calls().empty());
        QVERIFY(!audio.running());
    }

    void paintFailureStopsTheAnimationAndClosesEverything()
    {
        auto& controller = rig->controller;
        auto& keyboard = rig->keyboard;
        auto& audio = rig->audio;
        auto& clock = rig->clock;
        auto& registry = rig->registry;
        (void)keyboard;
        (void)audio;
        (void)clock;
        (void)registry;
        (void)controller;
        keyboard.failPaint({ErrorCode::BackendUnavailable, "teclado sumiu"});

        controller->start("audio", Qt::green, Qt::blue, 6, false, 50, 20);

        QTRY_VERIFY_WITH_TIMEOUT(controller->status().contains("indispon"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->running() && !controller->busy(), 5000);
        QVERIFY(!audio.running());
    }

    void startWhileRunningIsIgnored()
    {
        auto& controller = rig->controller;
        auto& keyboard = rig->keyboard;
        auto& audio = rig->audio;
        auto& clock = rig->clock;
        auto& registry = rig->registry;
        (void)keyboard;
        (void)audio;
        (void)clock;
        (void)registry;
        (void)controller;
        controller->start("cycle", Qt::red, Qt::blue, 6, false, 50, 10);
        QTRY_VERIFY_WITH_TIMEOUT(controller->running(), 5000);

        controller->start("wave", Qt::red, Qt::blue, 6, false, 50, 10);
        controller->stop();
        QTRY_VERIFY_WITH_TIMEOUT(!controller->running() && !controller->busy(), 5000);

        QCOMPARE(countOf(keyboard.calls(), "open"), 1);
    }

    void releaseForSleepReturnsControlSynchronously()
    {
        auto& controller = rig->controller;
        auto& keyboard = rig->keyboard;
        auto& audio = rig->audio;
        auto& clock = rig->clock;
        auto& registry = rig->registry;
        (void)keyboard;
        (void)audio;
        (void)clock;
        (void)registry;
        (void)controller;
        controller->start("stars", Qt::white, Qt::blue, 6, false, 30, 10);
        QTRY_VERIFY_WITH_TIMEOUT(controller->running(), 5000);

        controller->releaseForSleep();

        QVERIFY(!controller->running());
        QCOMPARE(countOf(keyboard.calls(), "end"), 1);
    }

    void destroyingWhileRunningReturnsControl()
    {
        auto& controller = rig->controller;
        auto& keyboard = rig->keyboard;
        auto& audio = rig->audio;
        auto& clock = rig->clock;
        auto& registry = rig->registry;
        (void)keyboard;
        (void)audio;
        (void)clock;
        (void)registry;
        (void)controller;
        controller->start("keyframes", Qt::red, Qt::blue, 6, false, 50, 10);
        QTRY_VERIFY_WITH_TIMEOUT(controller->running(), 5000);

        controller.reset();

        QCOMPARE(countOf(keyboard.calls(), "end"), 1);
    }

private:
    // Cada teste recebe fakes novos (QTest reutiliza o objeto de teste entre os slots).
    struct Rig {
        ghub::fakes::FakeClock clock;
        AccessRegistry registry{clock};
        ghub::fakes::FakeExternalProcessProbe probe;
        ghub::fakes::FakeKeyboardLighting keyboard;
        ghub::fakes::FakeAudioLevelSource audio;
        StartLiveKeyboardSession startUseCase{keyboard, registry, probe, g915Interface()};
        std::unique_ptr<SoftwareAnimationController> controller =
            std::make_unique<SoftwareAnimationController>(startUseCase, clock, audio, 7U);
    };
    std::unique_ptr<Rig> rig;
};

QTEST_MAIN(SoftwareAnimationControllerTest)
#include "SoftwareAnimationControllerTest.moc"
