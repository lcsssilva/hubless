#include "modules/lighting/presentation/G915OnboardController.hpp"

#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeOnboardMemory.hpp"
#include "fakes/G915SectorFixtures.hpp"

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
using ghub::lighting::application::G915OnboardSettings;
using ghub::lighting::presentation::G915OnboardController;
namespace fx = ghub::fixtures::g915;

namespace {

using Bytes = std::vector<std::uint8_t>;

template <std::size_t N> Bytes bytes(const std::array<std::uint8_t, N>& array)
{
    return {array.begin(), array.end()};
}

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

QVariantMap effect(const QString& kind, const QString& color = QStringLiteral("#000000"), int periodMs = 0,
                   int brightness = 0, int speed = 0)
{
    return {{QStringLiteral("kind"), kind},
            {QStringLiteral("color"), color},
            {QStringLiteral("periodMs"), periodMs},
            {QStringLiteral("brightness"), brightness},
            {QStringLiteral("speed"), speed},
            {QStringLiteral("raw"), QString{}}};
}

QVariantMap gKey(const QString& kind, int modifiers = 0, const QString& key = {}, const QString& raw = {})
{
    return {{QStringLiteral("kind"), kind},
            {QStringLiteral("modifiers"), modifiers},
            {QStringLiteral("key"), key},
            {QStringLiteral("raw"), raw}};
}

QVariantList keepGKeys(const G915OnboardController& controller)
{
    return controller.gKeys();
}

class G915OnboardControllerTest : public QObject {
    Q_OBJECT

private slots:
    // Fixture nova a cada teste: os membros de um QObject de teste sobreviveriam entre os slots.
    void init() { fixture = std::make_unique<Fixture>(); }

    void cleanup() { fixture.reset(); }

    void construction_CallsNothingAndStartsEmpty()
    {
        const G915OnboardController controller{fixture->useCase};

        QCOMPARE(fixture->probe.calls(), 0);
        QCOMPARE(fixture->connector.state.opens, 0);
        QVERIFY(!controller.busy());
        QVERIFY(!controller.loaded());
        QVERIFY(!controller.backupExists());
        QVERIFY(controller.status().isEmpty());
        QCOMPARE(controller.gKeys().size(), 0);
    }

    void keyChoices_ListOnlyKeyboardKeysWithoutThePrefix()
    {
        const G915OnboardController controller{fixture->useCase};

        const auto choices = controller.keyChoices();
        QVERIFY(choices.contains(QStringLiteral("C")));
        QVERIFY(choices.contains(QStringLiteral("F1")));
        QVERIFY(!choices.contains(QStringLiteral("G1")));
        QVERIFY(!choices.contains(QStringLiteral("Left Control")));
        QVERIFY(!choices.contains(QStringLiteral("Logo")));
    }

    void readFromKeyboard_PopulatesPropertiesOffTheUiThread()
    {
        G915OnboardController controller{fixture->useCase};

        controller.readFromKeyboard();
        QVERIFY(controller.busy());
        QTRY_VERIFY(!controller.busy());

        QVERIFY(controller.loaded());
        QCOMPARE(controller.activeSector(), 1);
        QVERIFY(!controller.backupExists());
        QCOMPARE(controller.logoEffect().value(QStringLiteral("kind")).toString(), QStringLiteral("static"));
        QCOMPARE(controller.logoEffect().value(QStringLiteral("color")).toString(), QStringLiteral("#000000"));
        const auto keys = controller.keysEffect();
        QCOMPARE(keys.value(QStringLiteral("kind")).toString(), QStringLiteral("ripple"));
        QCOMPARE(keys.value(QStringLiteral("speed")).toInt(), 20);
        const auto g = controller.gKeys();
        QCOMPARE(g.size(), 5);
        QCOMPARE(g.at(0).toMap().value(QStringLiteral("kind")).toString(), QStringLiteral("key"));
        QCOMPARE(g.at(0).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("F1"));
        QCOMPARE(g.at(0).toMap().value(QStringLiteral("modifiers")).toInt(), 0);
        QCOMPARE(g.at(4).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("F5"));
        QCOMPARE(fixture->connector.state.count("write:1"), 0);
        QVERIFY(!controller.status().isEmpty());
    }

    void readFromKeyboard_FailureReportsTheReasonAndStaysUnloaded()
    {
        fixture->probe.setResult(std::vector<ExternalOwner>{ExternalOwner::Piper});
        G915OnboardController controller{fixture->useCase};

        controller.readFromKeyboard();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(!controller.loaded());
        QVERIFY(controller.status().contains(QStringLiteral("piper")));
    }

    void readFromKeyboard_UnknownEffectFormatIsPublishedAsRaw()
    {
        Bytes raw = bytes(fx::kBeforeC04M1);
        raw[0xD0] = 0x05; // efeito sem formato confirmado na entrada do logo
        fixture->connector.state.sectors[1] = ghub::onboard_memory::OnboardSector::fromBytes(raw)->withCrc().bytes();
        G915OnboardController controller{fixture->useCase};

        controller.readFromKeyboard();
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(controller.logoEffect().value(QStringLiteral("kind")).toString(), QStringLiteral("raw"));
    }

    void writeToKeyboard_G1AsCtrlCStoresTheExactBytesAndPublishesWhatWasReadBack()
    {
        G915OnboardController controller{fixture->useCase};
        controller.readFromKeyboard();
        QTRY_VERIFY(!controller.busy());
        auto gKeys = keepGKeys(controller);
        gKeys[0] = gKey(QStringLiteral("key"), 1, QStringLiteral("C"));

        controller.writeToKeyboard(controller.logoEffect(), controller.keysEffect(), gKeys);
        QVERIFY(controller.busy());
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->connector.state.count("write:1"), 1);
        const auto& after = fixture->connector.state.sectors.at(1);
        QCOMPARE((Bytes{after.begin() + 0x20, after.begin() + 0x24}), (Bytes{0x80, 0x02, 0x01, 0x06}));
        QCOMPARE(controller.gKeys().at(0).toMap().value(QStringLiteral("modifiers")).toInt(), 1);
        QCOMPARE(controller.gKeys().at(0).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("C"));
        QVERIFY(controller.backupExists());
        QVERIFY(controller.status().contains(QStringLiteral("conferido")));
    }

    void writeToKeyboard_EffectsAreWrittenAsTheGHubDid()
    {
        G915OnboardController controller{fixture->useCase};
        controller.readFromKeyboard();
        QTRY_VERIFY(!controller.busy());

        controller.writeToKeyboard(effect(QStringLiteral("cycle"), {}, 5000, 100),
                                   effect(QStringLiteral("wave"), {}, 5000, 100), keepGKeys(controller));
        QTRY_VERIFY(!controller.busy());

        const auto& after = fixture->connector.state.sectors.at(1);
        const auto target = bytes(fx::kAfterC06M1);
        QCOMPARE((Bytes{after.begin() + 0xD0, after.begin() + 0xFC}),
                 (Bytes{target.begin() + 0xD0, target.begin() + 0xFC}));
        QCOMPARE(controller.logoEffect().value(QStringLiteral("kind")).toString(), QStringLiteral("cycle"));
        QCOMPARE(controller.keysEffect().value(QStringLiteral("kind")).toString(), QStringLiteral("wave"));
        QCOMPARE(controller.keysEffect().value(QStringLiteral("periodMs")).toInt(), 5000);
    }

    void writeToKeyboard_InvalidValuesDoNotCallThePortAndExplainInStatus()
    {
        G915OnboardController controller{fixture->useCase};
        controller.readFromKeyboard();
        QTRY_VERIFY(!controller.busy());
        const int opensAfterRead = fixture->connector.state.opens;
        const auto okLogo = effect(QStringLiteral("static"));
        const auto okKeys = effect(QStringLiteral("static"));
        const auto okKeysList = keepGKeys(controller);

        const std::vector<std::pair<QVariantMap, QVariantMap>> badEffects{
            {effect(QStringLiteral("wave"), {}, 2100, 100), okKeys},              // logo sem onda
            {effect(QStringLiteral("ripple"), {}, 0, 0, 20), okKeys},             // logo sem ondulação
            {effect(QStringLiteral("off")), okKeys},                              // sem formato gravado
            {okLogo, effect(QStringLiteral("off"))},                              // teclas sem "off"
            {effect(QStringLiteral("cycle"), {}, 100, 100), okKeys},              // período curto demais
            {effect(QStringLiteral("cycle"), {}, 5000, 101), okKeys},             // brilho > 100
            {effect(QStringLiteral("static"), QStringLiteral("00ffff")), okKeys}, // cor sem #
            {okLogo, effect(QStringLiteral("ripple"), {}, 0, 0, 1)},              // velocidade fora de 2..200
            {okLogo, effect(QStringLiteral("desconhecido"))},                     // tipo inexistente
        };
        for (const auto& [logo, keys] : badEffects) {
            controller.writeToKeyboard(logo, keys, okKeysList);
            QVERIFY(!controller.busy());
            QVERIFY2(controller.status().startsWith(QStringLiteral("Valores inválidos")), qPrintable(controller.status()));
        }

        auto badKeys = okKeysList;
        const std::vector<QVariantMap> badGKeys{
            gKey(QStringLiteral("key"), 16, QStringLiteral("C")),                 // modificador fora do bitmap
            gKey(QStringLiteral("key"), 1, QStringLiteral("G1")),                 // tecla fora do teclado
            gKey(QStringLiteral("key"), 1, QStringLiteral("Left Control")),       // modificador como tecla
            gKey(QStringLiteral("key"), 1, QStringLiteral("inexistente")),
            gKey(QStringLiteral("raw"), 0, {}, QStringLiteral("90 0d ff")),       // bruto com 3 bytes
            gKey(QStringLiteral("desconhecido")),
        };
        for (const auto& bad : badGKeys) {
            badKeys[0] = bad;
            controller.writeToKeyboard(controller.logoEffect(), controller.keysEffect(), badKeys);
            QVERIFY(!controller.busy());
            QVERIFY2(controller.status().startsWith(QStringLiteral("Valores inválidos")), qPrintable(controller.status()));
        }
        QCOMPARE(controller.gKeys().size(), 5);
        badKeys.removeLast();
        controller.writeToKeyboard(controller.logoEffect(), controller.keysEffect(), badKeys);
        QVERIFY(!controller.busy()); // lista com 4 teclas
        QVERIFY2(controller.status().startsWith(QStringLiteral("Valores inválidos")), qPrintable(controller.status()));

        QCOMPARE(fixture->connector.state.opens, opensAfterRead);
        QCOMPARE(fixture->connector.state.count("write:1"), 0);
    }

    void writeToKeyboard_RawEntriesSentBackAreLeftUntouched()
    {
        Bytes raw = bytes(fx::kBeforeC04M1);
        raw[0xD0] = 0x05;
        raw[0xE6] = 0x05;
        raw[0x20] = 0x90; // G1 num formato bruto
        raw[0x21] = 0x0D;
        raw[0x22] = 0xFF;
        raw[0x23] = 0x01;
        const auto stored = ghub::onboard_memory::OnboardSector::fromBytes(raw)->withCrc().bytes();
        fixture->connector.state.sectors[1] = stored;
        G915OnboardController controller{fixture->useCase};
        controller.readFromKeyboard();
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.gKeys().at(0).toMap().value(QStringLiteral("kind")).toString(), QStringLiteral("raw"));

        controller.writeToKeyboard(controller.logoEffect(), controller.keysEffect(), keepGKeys(controller));
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->connector.state.count("write:1"), 0);
        QCOMPARE(fixture->connector.state.sectors.at(1), stored);
    }

    void writeToKeyboard_DefaultBindingGoesBackToTheBackup()
    {
        fixture->connector.state.sectors[1] = bytes(fx::kAfterC06M1);
        fixture->backup.stored[1] = bytes(fx::kBeforeC04M1);
        G915OnboardController controller{fixture->useCase};
        controller.readFromKeyboard();
        QTRY_VERIFY(!controller.busy());
        QVERIFY(controller.backupExists());
        auto gKeys = keepGKeys(controller);
        gKeys[0] = gKey(QStringLiteral("default"));

        controller.writeToKeyboard(controller.logoEffect(), controller.keysEffect(), gKeys);
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(controller.gKeys().at(0).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("F1"));
        QCOMPARE(controller.gKeys().at(0).toMap().value(QStringLiteral("modifiers")).toInt(), 0);
    }

    void writeToKeyboard_SameSettingsPublishesTheRealBackupStateAndSector()
    {
        G915OnboardController controller{fixture->useCase};
        controller.readFromKeyboard();
        QTRY_VERIFY(!controller.busy());

        controller.writeToKeyboard(controller.logoEffect(), controller.keysEffect(), keepGKeys(controller));
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->connector.state.count("write:1"), 0);
        QVERIFY(!controller.backupExists());
        QCOMPARE(controller.activeSector(), 1);
    }

    void writeToKeyboard_FailureReportsTheReason()
    {
        G915OnboardController controller{fixture->useCase};
        controller.readFromKeyboard();
        QTRY_VERIFY(!controller.busy());
        fixture->connector.state.mode = 2; // fora do modo onboard: a escrita é recusada
        auto gKeys = keepGKeys(controller);
        gKeys[1] = gKey(QStringLiteral("key"), 2, QStringLiteral("A"));

        controller.writeToKeyboard(controller.logoEffect(), controller.keysEffect(), gKeys);
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->connector.state.count("write:1"), 0);
        QVERIFY(controller.status().startsWith(QStringLiteral("Falha")));
    }

    void request_DuringBusyIsRefusedAndNotQueued()
    {
        Gate gate;
        fixture->backup.onSave = [&gate] { gate.wait(); };
        G915OnboardController controller{fixture->useCase};
        controller.readFromKeyboard();
        QTRY_VERIFY(!controller.busy());
        auto gKeys = keepGKeys(controller);
        gKeys[0] = gKey(QStringLiteral("key"), 1, QStringLiteral("C"));
        controller.writeToKeyboard(controller.logoEffect(), controller.keysEffect(), gKeys);
        QTRY_VERIFY(gate.entered());

        gKeys[0] = gKey(QStringLiteral("key"), 4, QStringLiteral("Z"));
        controller.writeToKeyboard(controller.logoEffect(), controller.keysEffect(), gKeys);
        QCOMPARE(controller.status(), QStringLiteral("aguarde a operação em andamento"));
        controller.readFromKeyboard();
        controller.restoreBackup();
        QCOMPARE(controller.status(), QStringLiteral("aguarde a operação em andamento"));

        gate.open();
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(fixture->connector.state.count("write:1"), 1);
        QCOMPARE(controller.gKeys().at(0).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("C"));
    }

    void restoreBackup_WithoutBackupReportsIt()
    {
        G915OnboardController controller{fixture->useCase};

        controller.restoreBackup();
        QTRY_VERIFY(!controller.busy());

        QVERIFY(controller.status().contains(QStringLiteral("sem cópia de segurança")));
        QCOMPARE(fixture->connector.state.count("write:1"), 0);
    }

    void restoreBackup_RestoresAndPublishesTheOriginal()
    {
        G915OnboardController controller{fixture->useCase};
        controller.readFromKeyboard();
        QTRY_VERIFY(!controller.busy());
        auto gKeys = keepGKeys(controller);
        gKeys[0] = gKey(QStringLiteral("key"), 1, QStringLiteral("C"));
        controller.writeToKeyboard(controller.logoEffect(), controller.keysEffect(), gKeys);
        QTRY_VERIFY(!controller.busy());

        controller.restoreBackup();
        QTRY_VERIFY(!controller.busy());

        QCOMPARE(fixture->connector.state.sectors.at(1), bytes(fx::kBeforeC04M1));
        QCOMPARE(controller.gKeys().at(0).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("F1"));
        QVERIFY(controller.backupExists());
    }

    void destructor_WaitsForTheOperationInProgress()
    {
        Gate gate;
        fixture->backup.onSave = [&gate] { gate.wait(); };
        auto controller = std::make_unique<G915OnboardController>(fixture->useCase);
        controller->readFromKeyboard();
        QTRY_VERIFY(!controller->busy());
        auto gKeys = controller->gKeys();
        gKeys[0] = gKey(QStringLiteral("key"), 1, QStringLiteral("C"));
        controller->writeToKeyboard(controller->logoEffect(), controller->keysEffect(), gKeys);
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
        G915OnboardSettings useCase{probe, registry, connector, backup};

        Fixture()
        {
            probe.setResult(std::vector<ExternalOwner>{});
            connector.state.layout = {.sectorSize = 255, .profileCount = 3};
            connector.state.sectors[0] = bytes(fx::kDirectory);
            connector.state.sectors[1] = bytes(fx::kBeforeC04M1);
            connector.state.sectors[2] = bytes(fx::kBeforeC04M2);
            connector.state.sectors[3] = bytes(fx::kBeforeC04M3);
        }
    };

    std::unique_ptr<Fixture> fixture;
};

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): o construtor do teste usa Result::value() em dado constante válido.
QTEST_GUILESS_MAIN(G915OnboardControllerTest)
#include "G915OnboardControllerTest.moc"
