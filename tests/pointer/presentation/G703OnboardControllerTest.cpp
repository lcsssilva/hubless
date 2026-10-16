#include "modules/pointer/presentation/G703OnboardController.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeOnboardMemory.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <QSignalSpy>
#include <QTest>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

using ghub::device_access::AccessRegistry;
using ghub::device_access::ExternalOwner;
using ghub::pointer::G703OnboardController;
using ghub::pointer::G703OnboardSettings;

namespace {

using Bytes = std::vector<std::uint8_t>;

// Porta que bloqueia a thread de trabalho até o teste liberar (sem sleep).
class Gate {
public:
    void wait()
    {
        entered_.store(true);
        std::unique_lock lock{mutex_};
        condition_.wait(lock, [this] { return open_; });
    }

    void open()
    {
        {
            const std::scoped_lock lock{mutex_};
            open_ = true;
        }
        condition_.notify_all();
    }

    [[nodiscard]] bool entered() const { return entered_.load(); }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool open_ = false;
    std::atomic<bool> entered_{false};
};

Bytes sector0001()
{
    return {ghub::fixtures::kSector0001.begin(), ghub::fixtures::kSector0001.end()};
}

QVariantMap led(const QString& kind, const QString& color = {}, int periodMs = 0, int brightness = 0)
{
    return {{QStringLiteral("kind"), kind},
            {QStringLiteral("color"), color},
            {QStringLiteral("periodMs"), periodMs},
            {QStringLiteral("brightness"), brightness},
            {QStringLiteral("raw"), QString{}}};
}

QVariantMap offLed()
{
    return led(QStringLiteral("off"));
}

QVariantList stages(std::initializer_list<int> values)
{
    QVariantList list;
    for (const int value : values) {
        list.append(value);
    }
    return list;
}

class G703OnboardControllerTest : public QObject {
    Q_OBJECT

private slots:
    // Fixture nova a cada teste: os membros de um QObject de teste sobreviveriam entre os slots.
    void init() { fixture = std::make_unique<Fixture>(); }

    void cleanup() { fixture.reset(); }

    void construction_CallsNothingAndStartsEmpty()
    {
        const G703OnboardController controller{fixture->useCase};

        QCOMPARE(fixture->probe.calls(), 0);
        QCOMPARE(fixture->connector.state.opens, 0);
        QVERIFY(!controller.busy());
        QVERIFY(!controller.loaded());
        QVERIFY(!controller.backupExists());
        QVERIFY(controller.status().isEmpty());
        QCOMPARE(controller.reportRateHz(), 0);
        QVERIFY(controller.dpiStages().isEmpty());
    }

    void readFromMouse_PopulatesPropertiesOffTheUiThread()
    {
        G703OnboardController controller{fixture->useCase};

        controller.readFromMouse();
        QVERIFY(controller.busy());
        QTRY_VERIFY(!controller.busy());

        QVERIFY(controller.loaded());
        QCOMPARE(controller.reportRateHz(), 1000);
        QCOMPARE(controller.dpiStages(), stages({7050}));
        QCOMPARE(controller.defaultStage(), 0);
        QCOMPARE(controller.activeSector(), 1);
        QVERIFY(!controller.backupExists());
        QVERIFY(!controller.status().isEmpty());
        QCOMPARE(fixture->connector.state.count("write:1"), 0);
    }

    void readFromMouse_FailureReportsTheReasonAndStaysUnloaded()
    {
        fixture->probe.setResult(std::vector<ExternalOwner>{ExternalOwner::Piper});
        G703OnboardController controller{fixture->useCase};

        controller.readFromMouse();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(!controller.loaded());
        QVERIFY(controller.status().contains(QStringLiteral("piper")));
    }

    void writeToMouse_InvalidDpiDoesNotCallThePortAndExplainsInStatus()
    {
        G703OnboardController controller{fixture->useCase};

        controller.writeToMouse(1000, stages({7025}), 0, 0, offLed(), offLed());

        QVERIFY(!controller.busy());
        QVERIFY(!controller.status().isEmpty());
        QCOMPARE(fixture->probe.calls(), 0);
        QCOMPARE(fixture->connector.state.opens, 0);
    }

    void writeToMouse_ValidWritesAndPublishesWhatWasReadBack()
    {
        G703OnboardController controller{fixture->useCase};

        controller.writeToMouse(500, stages({7050, 1600}), 1, 0, offLed(), offLed());
        QVERIFY(controller.busy());
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->connector.state.count("write:1"), 1);
        QVERIFY(controller.loaded());
        QCOMPARE(controller.reportRateHz(), 500);
        QCOMPARE(controller.dpiStages(), stages({7050, 1600}));
        QCOMPARE(controller.defaultStage(), 1);
        QVERIFY(controller.backupExists());
        QVERIFY(!controller.status().isEmpty());
    }

    // Gravar o que o mouse já tem não escreve nem cria cópia: "Restaurar cópia" não pode ficar habilitado.
    void writeToMouse_SameSettingsPublishesTheRealBackupStateAndSector()
    {
        G703OnboardController controller{fixture->useCase};

        controller.writeToMouse(1000, stages({7050}), 0, 0, offLed(), offLed());
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->connector.state.count("write:1"), 0);
        QVERIFY(controller.loaded());
        QVERIFY(!controller.backupExists());
        QCOMPARE(controller.activeSector(), 1);
    }

    void writeToMouse_FailureReportsTheReason()
    {
        fixture->connector.state.mode = 2; // fora do modo onboard: a escrita é recusada
        G703OnboardController controller{fixture->useCase};

        controller.writeToMouse(500, stages({7050}), 0, 0, offLed(), offLed());
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->connector.state.count("write:1"), 0);
        QVERIFY(!controller.status().isEmpty());
        QVERIFY(!controller.backupExists());
    }

    void readFromMouse_PublishesShiftStageAndLeds()
    {
        G703OnboardController controller{fixture->useCase};

        controller.readFromMouse();
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(controller.dpiShiftStage(), 0);
        QCOMPARE(controller.primaryLed().value(QStringLiteral("kind")).toString(), QStringLiteral("off"));
        QCOMPARE(controller.logoLed().value(QStringLiteral("kind")).toString(), QStringLiteral("off"));
    }

    void readFromMouse_PublishesCycleLedWithPeriodAndBrightnessAndNoShiftAsMinusOne()
    {
        Bytes raw = sector0001();
        raw[2] = 0xff; // sem shift
        const Bytes cycle{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x40, 0x64, 0x00, 0x00};
        std::copy(cycle.begin(), cycle.end(), raw.begin() + 0xD0);
        fixture->connector.state.sectors[1] = ghub::onboard_memory::OnboardSector::fromBytes(raw)->withCrc().bytes();
        G703OnboardController controller{fixture->useCase};

        controller.readFromMouse();
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(controller.dpiShiftStage(), -1);
        const auto primary = controller.primaryLed();
        QCOMPARE(primary.value(QStringLiteral("kind")).toString(), QStringLiteral("cycle"));
        QCOMPARE(primary.value(QStringLiteral("periodMs")).toInt(), 8000);
        QCOMPARE(primary.value(QStringLiteral("brightness")).toInt(), 100);
    }

    void readFromMouse_UnknownLedFormatIsPublishedAsRawHex()
    {
        Bytes raw = sector0001();
        const Bytes ripple{0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00};
        std::copy(ripple.begin(), ripple.end(), raw.begin() + 0xD0);
        fixture->connector.state.sectors[1] = ghub::onboard_memory::OnboardSector::fromBytes(raw)->withCrc().bytes();
        G703OnboardController controller{fixture->useCase};

        controller.readFromMouse();
        QTRY_VERIFY(!controller.busy());

        const auto primary = controller.primaryLed();
        QCOMPARE(primary.value(QStringLiteral("kind")).toString(), QStringLiteral("raw"));
        QCOMPARE(primary.value(QStringLiteral("raw")).toString(), QStringLiteral("0b 00 00 00 00 00 14 00 00 00 00"));
    }

    void writeToMouse_WritesShiftAndBothLedsAndPublishesWhatWasReadBack()
    {
        G703OnboardController controller{fixture->useCase};

        controller.writeToMouse(500, stages({400, 1600, 3200}), 2, 1,
                                led(QStringLiteral("fixed"), QStringLiteral("#00ffff")),
                                led(QStringLiteral("cycle"), {}, 5000, 80));
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->connector.state.count("write:1"), 1);
        QCOMPARE(controller.dpiShiftStage(), 1);
        QCOMPARE(controller.primaryLed().value(QStringLiteral("kind")).toString(), QStringLiteral("fixed"));
        QCOMPARE(controller.primaryLed().value(QStringLiteral("color")).toString(), QStringLiteral("#00ffff"));
        QCOMPARE(controller.logoLed().value(QStringLiteral("kind")).toString(), QStringLiteral("cycle"));
        QCOMPARE(controller.logoLed().value(QStringLiteral("periodMs")).toInt(), 5000);
        QCOMPARE(controller.logoLed().value(QStringLiteral("brightness")).toInt(), 80);
    }

    void writeToMouse_RawLedSentBackIsLeftUntouched()
    {
        Bytes raw = sector0001();
        const Bytes ripple{0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00};
        std::copy(ripple.begin(), ripple.end(), raw.begin() + 0xD0);
        fixture->connector.state.sectors[1] = ghub::onboard_memory::OnboardSector::fromBytes(raw)->withCrc().bytes();
        G703OnboardController controller{fixture->useCase};
        controller.readFromMouse();
        QTRY_VERIFY(!controller.busy());

        controller.writeToMouse(1000, stages({7050}), 0, 0, controller.primaryLed(), controller.logoLed());
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->connector.state.count("write:1"), 0);
        QCOMPARE(fixture->connector.state.sectors.at(1),
                 ghub::onboard_memory::OnboardSector::fromBytes(raw)->withCrc().bytes());
    }

    // -1 = "sem shift", que ainda não tem formato confirmado: com o shift válido no setor, nada é gravado e o status
    // explica.
    void writeToMouse_ShiftMinusOneIsRefusedWhileTheNoShiftFormatIsUnconfirmed()
    {
        G703OnboardController controller{fixture->useCase};

        controller.writeToMouse(1000, stages({400, 800}), 0, -1, offLed(), offLed());
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->connector.state.count("write:1"), 0);
        QVERIFY2(controller.status().contains(QStringLiteral("DPI shift")), qPrintable(controller.status()));
    }

    void writeToMouse_InvalidShiftOrLedDoesNotCallThePortAndExplainsInStatus()
    {
        G703OnboardController controller{fixture->useCase};
        const std::vector<std::pair<int, QVariantMap>> bad{
            {5, offLed()},                                               // shift fora dos estágios
            {-2, offLed()},                                              // shift negativo inválido
            {0, led(QStringLiteral("cycle"), {}, 5000, 101)},            // brilho > 100
            {0, led(QStringLiteral("cycle"), {}, 0, 50)},                // período 0
            {0, led(QStringLiteral("fixed"), QStringLiteral("00ffff"))}, // cor sem #
            {0, led(QStringLiteral("fixed"), QStringLiteral("#00ff"))},  // cor curta
            {0, led(QStringLiteral("desconhecido"))},                    // tipo inexistente
        };
        for (const auto& [shift, primary] : bad) {
            controller.writeToMouse(1000, stages({400, 800}), 0, shift, primary, offLed());
            QVERIFY(!controller.busy());
            QVERIFY(!controller.status().isEmpty());
        }
        QCOMPARE(fixture->probe.calls(), 0);
        QCOMPARE(fixture->connector.state.opens, 0);
    }

    void request_DuringBusyIsRefusedAndNotQueued()
    {
        Gate gate;
        fixture->backup.onSave = [&gate] { gate.wait(); };
        G703OnboardController controller{fixture->useCase};

        controller.writeToMouse(500, stages({7050, 1600}), 0, 0, offLed(), offLed());
        QTRY_VERIFY(gate.entered());
        controller.writeToMouse(250, stages({1000}), 0, 0, offLed(), offLed());
        QCOMPARE(controller.status(), QStringLiteral("aguarde a operação em andamento"));
        controller.readFromMouse();
        controller.restoreBackup();
        QCOMPARE(controller.status(), QStringLiteral("aguarde a operação em andamento"));

        gate.open();
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(fixture->connector.state.count("write:1"), 1); // o segundo pedido não foi enfileirado
        QCOMPARE(controller.reportRateHz(), 500);
    }

    void restoreBackup_WithoutBackupReportsIt()
    {
        G703OnboardController controller{fixture->useCase};

        controller.restoreBackup();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(controller.status().contains(QStringLiteral("sem cópia de segurança")));
        QCOMPARE(fixture->connector.state.count("write:1"), 0);
    }

    void restoreBackup_RestoresAndPublishesTheOriginal()
    {
        G703OnboardController controller{fixture->useCase};
        controller.writeToMouse(500, stages({7050, 1600}), 0, 0, offLed(), offLed());
        QTRY_VERIFY(!controller.busy());

        controller.restoreBackup();
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->connector.state.sectors.at(1), sector0001());
        QCOMPARE(controller.reportRateHz(), 1000);
        QCOMPARE(controller.dpiStages(), stages({7050}));
        QVERIFY(controller.backupExists());
    }

    void destructor_WaitsForTheOperationInProgress()
    {
        Gate gate;
        fixture->backup.onSave = [&gate] { gate.wait(); };
        auto controller = std::make_unique<G703OnboardController>(fixture->useCase);
        controller->writeToMouse(500, stages({7050, 1600}), 0, 0, offLed(), offLed());
        QTRY_VERIFY(gate.entered());

        std::thread opener{[&gate] { gate.open(); }};
        controller.reset(); // só retorna depois que a gravação terminou
        opener.join();

        QCOMPARE(fixture->connector.state.count("write:1"), 1);
        QCOMPARE(fixture->connector.state.liveMemories, 0);
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    struct Fixture {
        ghub::fakes::FakeClock clock;
        AccessRegistry registry{clock};
        ghub::fakes::FakeExternalProcessProbe probe;
        ghub::fakes::FakeOnboardMemoryConnector connector;
        ghub::fakes::FakeOnboardBackupStore backup;
        G703OnboardSettings useCase{probe, registry, connector, backup};

        Fixture()
        {
            probe.setResult(std::vector<ExternalOwner>{});
            connector.state.sectors[0] = {ghub::fixtures::kDirectory0000.begin(), ghub::fixtures::kDirectory0000.end()};
            connector.state.sectors[1] = sector0001();
            connector.state.sectors[2] = {ghub::fixtures::kSector0002.begin(), ghub::fixtures::kSector0002.end()};
        }
    };

    std::unique_ptr<Fixture> fixture;
};

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): o construtor do teste usa Result::value() em dado constante válido.
QTEST_GUILESS_MAIN(G703OnboardControllerTest)
#include "G703OnboardControllerTest.moc"
