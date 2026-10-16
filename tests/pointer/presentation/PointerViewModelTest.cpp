#include "modules/pointer/presentation/PointerViewModel.hpp"
#include "fakes/FakePointerReadRunner.hpp"

#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

#include <optional>
#include <utility>
#include <vector>

using ghub::device_inventory::ConnectionPath;
using ghub::pointer::ButtonAction;
using ghub::pointer::ButtonActionKind;
using ghub::pointer::ButtonMapping;
using ghub::pointer::LedMode;
using ghub::pointer::LedState;
using ghub::pointer::NotDetected;
using ghub::pointer::PointerConfiguration;
using ghub::pointer::PointerProfile;
using ghub::pointer::PointerStatus;
using ghub::pointer::PointerViewModel;
using ghub::pointer::Ready;
using ghub::pointer::ResolutionStage;
using ghub::pointer::Unavailable;
using ghub::shared_kernel::ErrorCode;

namespace {

ResolutionStage stage(std::uint32_t index, std::uint32_t dpiX, std::optional<std::uint32_t> dpiY, bool active,
                      bool isDefault, std::vector<std::uint32_t> supported)
{
    return ResolutionStage::create({
                                       .index = index,
                                       .dpiX = dpiX,
                                       .dpiY = dpiY,
                                       .active = active,
                                       .isDefault = isDefault,
                                       .disabled = false,
                                       .supportedDpis = std::move(supported),
                                   })
        .value();
}

ButtonMapping button(std::uint32_t index, ButtonAction action)
{
    return ButtonMapping::create({.index = index, .action = action, .supportedKinds = {ButtonActionKind::Button}})
        .value();
}

LedState led(std::uint32_t index, LedMode mode, std::uint32_t rawMode, std::uint32_t r, std::uint32_t g,
             std::uint32_t b, std::uint32_t brightness)
{
    return LedState::create({
                                .index = index,
                                .mode = mode,
                                .rawMode = rawMode,
                                .red = r,
                                .green = g,
                                .blue = b,
                                .colorDepth = 1,
                                .brightness = brightness,
                                .effectDuration = 0,
                                .supportedModes = {LedMode::Off, LedMode::On},
                            })
        .value();
}

PointerProfile profile(std::uint32_t index, bool active, bool enabled, std::vector<ResolutionStage> stages = {},
                       std::vector<ButtonMapping> buttons = {}, std::vector<LedState> leds = {})
{
    return PointerProfile::create({
                                      .index = index,
                                      .name = "",
                                      .active = active,
                                      .enabled = enabled,
                                      .reportRate = 1000,
                                      .supportedReportRates = {125, 250, 500, 1000},
                                      .stages = std::move(stages),
                                      .buttons = std::move(buttons),
                                      .leds = std::move(leds),
                                  })
        .value();
}

// Valores da evidência do G703 LS (evidence/2026-10-13-ratbagd-leitura.md): cinco perfis; o ativo (1) vem
// desabilitado e o DPI 7050 do perfil 0 fica fora da lista sintética do ratbagd.
PointerStatus evidenceStatus()
{
    std::vector<PointerProfile> profiles;
    profiles.push_back(profile(0, false, true,
                               {stage(0, 7050, std::nullopt, true, true, {200, 400, 800, 1600, 3200, 6400})},
                               {button(5, ButtonAction::special(0x40000007)), button(0, ButtonAction::button(1))},
                               {led(0, LedMode::Off, 0, 0, 0, 0, 0)}));
    profiles.push_back(profile(1, true, false));
    for (std::uint32_t index = 2; index < 5; ++index) {
        profiles.push_back(profile(index, false, true));
    }
    return Ready{
        .configuration = PointerConfiguration::create("Logitech G703 LS", std::move(profiles)).value(),
        .path = ConnectionPath::LightspeedReceiver,
    };
}

PointerStatus readyWith(std::vector<PointerProfile> profiles, ConnectionPath path = ConnectionPath::UsbCable)
{
    return Ready{
        .configuration = PointerConfiguration::create("Logitech G703 LS", std::move(profiles)).value(),
        .path = path,
    };
}

QVariantMap profileMap(const QVariantList& list, int position)
{
    return list.at(position).toMap();
}

class PointerViewModelTest : public QObject {
    Q_OBJECT

private:
    ghub::fakes::FakePointerReadRunner* runner = nullptr;
    PointerViewModel* model = nullptr;

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private:` acima e `private slots:` são ambos privados.
private slots:
    void init()
    {
        runner = new ghub::fakes::FakePointerReadRunner;
        model = new PointerViewModel(*runner);
    }

    void cleanup()
    {
        delete model;
        delete runner;
    }

    void doesNotStartOnConstruction()
    {
        QCOMPARE(runner->startCalls(), 0);
        QVERIFY(!model->reading());
        QCOMPARE(model->state(), QStringLiteral("idle"));
        QVERIFY(model->profiles().isEmpty());
        QVERIFY(model->warnings().isEmpty());
        QVERIFY(model->deviceName().isEmpty());
        QVERIFY(model->connectionText().isEmpty());
    }

    void readStartsRunnerExactlyOnce_AndNotifies()
    {
        QSignalSpy changed(model, &PointerViewModel::pointerChanged);
        runner->setReading(true);

        model->read();

        QCOMPARE(runner->startCalls(), 1);
        QVERIFY(model->reading());
        QCOMPARE(model->state(), QStringLiteral("reading"));
        QCOMPARE(model->statusText(), QStringLiteral("lendo a configuração do mouse…"));
        QCOMPARE(changed.count(), 1);
    }

    void readFinishedNotDetected()
    {
        runner->setStatus(NotDetected{});
        QSignalSpy changed(model, &PointerViewModel::pointerChanged);

        model->onReadFinished();

        QCOMPARE(changed.count(), 1);
        QCOMPARE(model->state(), QStringLiteral("notDetected"));
        QCOMPARE(model->statusText(), QStringLiteral("mouse G703 não detectado"));
        QVERIFY(model->profiles().isEmpty());
        QVERIFY(!model->reading());
    }

    void readFinishedUnavailableIncludesErrorDetail()
    {
        runner->setStatus(Unavailable{{.code = ErrorCode::DeviceBusy, .detail = "em uso por openrgb"}});

        model->onReadFinished();

        QCOMPARE(model->state(), QStringLiteral("unavailable"));
        QCOMPARE(model->statusText(), QStringLiteral("leitura indisponível: em uso por openrgb"));
        QVERIFY(model->profiles().isEmpty());
        QVERIFY(model->warnings().isEmpty());
    }

    void readFinishedReadyShowsDeviceAndConnection()
    {
        runner->setStatus(evidenceStatus());

        model->onReadFinished();

        QCOMPARE(model->state(), QStringLiteral("ready"));
        QCOMPARE(model->statusText(), QStringLiteral("configuração lida"));
        QCOMPARE(model->deviceName(), QStringLiteral("Logitech G703 LS"));
        QCOMPARE(model->connectionText(), QStringLiteral("receptor Lightspeed"));
    }

    void connectionTextForCable()
    {
        runner->setStatus(readyWith({profile(0, true, true)}, ConnectionPath::UsbCable));
        model->onReadFinished();
        QCOMPARE(model->connectionText(), QStringLiteral("cabo USB"));
    }

    void profilesFollowEvidence()
    {
        runner->setStatus(evidenceStatus());
        model->onReadFinished();

        const auto profiles = model->profiles();
        QCOMPARE(profiles.size(), 5);
        const auto p0 = profileMap(profiles, 0);
        QCOMPARE(p0.value("index").toUInt(), 0U);
        QCOMPARE(p0.value("active").toBool(), false);
        QCOMPARE(p0.value("enabled").toBool(), true);
        QCOMPARE(p0.value("reportRate").toUInt(), 1000U);
        QCOMPARE(p0.value("supportedReportRates").toList(), (QVariantList{125, 250, 500, 1000}));

        const auto stages = p0.value("stages").toList();
        QCOMPARE(stages.size(), 1);
        const auto r0 = stages.at(0).toMap();
        QCOMPARE(r0.value("dpiText").toString(), QStringLiteral("7050 dpi"));
        QCOMPARE(r0.value("active").toBool(), true);
        QCOMPARE(r0.value("isDefault").toBool(), true);
        QCOMPARE(r0.value("disabled").toBool(), false);
        QCOMPARE(r0.value("listed").toBool(), false);

        const auto p1 = profileMap(profiles, 1);
        QCOMPARE(p1.value("active").toBool(), true);
        QCOMPARE(p1.value("enabled").toBool(), false);
    }

    void buttonsShowActionText()
    {
        runner->setStatus(evidenceStatus());
        model->onReadFinished();

        const auto buttons = profileMap(model->profiles(), 0).value("buttons").toList();
        QCOMPARE(buttons.size(), 2);
        QCOMPARE(buttons.at(0).toMap().value("index").toUInt(), 5U);
        QCOMPARE(buttons.at(0).toMap().value("text").toString(), QStringLiteral("especial 0x40000007"));
        QCOMPARE(buttons.at(1).toMap().value("text").toString(), QStringLiteral("botão 1"));
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): o Qt Test exige função de dados de instância.
    void buttonActionTexts_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<unsigned>("value");
        QTest::addColumn<QString>("expected");
        QTest::newRow("none") << 0 << 0U << QStringLiteral("nenhuma");
        QTest::newRow("key") << 3 << 30U << QStringLiteral("tecla 30");
        QTest::newRow("macro") << 4 << 3U << QStringLiteral("macro (3 passos)");
        QTest::newRow("unknown") << 5 << 0U << QStringLiteral("desconhecida");
        QTest::newRow("keyWithModifiers") << 3 << 0x0106U << QStringLiteral("tecla 6 + modificadores 0x01");
        QTest::newRow("unrecognizedBytes") << 6 << 0x90050000U << QStringLiteral("não interpretado: 90 05 00 00");
    }

    void buttonActionTexts()
    {
        QFETCH(int, kind);
        QFETCH(unsigned, value);
        QFETCH(QString, expected);
        ButtonAction action = ButtonAction::none();
        switch (kind) {
        case 3:
            action = ButtonAction::key(value);
            break;
        case 4:
            action = ButtonAction::macro(value);
            break;
        case 5:
            action = ButtonAction::unknown();
            break;
        case 6:
            action = ButtonAction::unrecognized(value);
            break;
        default:
            break;
        }
        runner->setStatus(readyWith({profile(0, true, true, {}, {button(0, action)})}));

        model->onReadFinished();

        const auto buttons = profileMap(model->profiles(), 0).value("buttons").toList();
        QCOMPARE(buttons.at(0).toMap().value("text").toString(), expected);
    }

    void ledOffShowsModeColorAndBrightness()
    {
        runner->setStatus(evidenceStatus());
        model->onReadFinished();

        const auto leds = profileMap(model->profiles(), 0).value("leds").toList();
        QCOMPARE(leds.size(), 1);
        const auto led0 = leds.at(0).toMap();
        QCOMPARE(led0.value("mode").toString(), QStringLiteral("desligado"));
        QCOMPARE(led0.value("color").toString(), QStringLiteral("#000000"));
        QCOMPARE(led0.value("brightness").toUInt(), 0U);
    }

    void ledModesAndColorFormatting()
    {
        runner->setStatus(readyWith({
            profile(0, true, true, {}, {},
                    {
                        led(0, LedMode::On, 1, 255, 128, 1, 200),
                        led(1, LedMode::Cycle, 2, 0, 0, 0, 10),
                        led(2, LedMode::Breathing, 3, 0, 0, 0, 10),
                        led(3, LedMode::Unknown, 9, 0, 0, 0, 10),
                    }),
        }));
        model->onReadFinished();

        const auto leds = profileMap(model->profiles(), 0).value("leds").toList();
        QCOMPARE(leds.at(0).toMap().value("mode").toString(), QStringLiteral("ligado"));
        QCOMPARE(leds.at(0).toMap().value("color").toString(), QStringLiteral("#ff8001"));
        QCOMPARE(leds.at(1).toMap().value("mode").toString(), QStringLiteral("ciclo"));
        QCOMPARE(leds.at(2).toMap().value("mode").toString(), QStringLiteral("respiração"));
        QCOMPARE(leds.at(3).toMap().value("mode").toString(), QStringLiteral("desconhecido (9)"));
    }

    void ledWithRawBytesShowsThemAsNotInterpreted()
    {
        const auto raw =
            LedState::create({.index = 0,
                              .mode = LedMode::Unknown,
                              .rawMode = 0x0b,
                              .red = 0,
                              .green = 0,
                              .blue = 0,
                              .colorDepth = 8,
                              .brightness = 0,
                              .effectDuration = 0,
                              .supportedModes = {},
                              .rawBytes = {0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00}})
                .value();
        runner->setStatus(readyWith({profile(0, true, true, {}, {}, {raw})}));
        model->onReadFinished();

        const auto leds = profileMap(model->profiles(), 0).value("leds").toList();
        QCOMPARE(leds.at(0).toMap().value("mode").toString(),
                 QStringLiteral("não interpretado: 0b 00 00 00 00 00 14 00 00 00 00"));
    }

    void separateXyDpiShowsBothAxes()
    {
        runner->setStatus(readyWith({profile(0, true, true, {stage(0, 800, 600, true, false, {800, 1600})})}));
        model->onReadFinished();

        const auto stages = profileMap(model->profiles(), 0).value("stages").toList();
        QCOMPARE(stages.at(0).toMap().value("dpiText").toString(), QStringLiteral("800×600 dpi"));
        QCOMPARE(stages.at(0).toMap().value("isDefault").toBool(), false);
    }

    void warningsDescribeDeviceAnomalies()
    {
        runner->setStatus(evidenceStatus());
        model->onReadFinished();

        QCOMPARE(model->warnings(),
                 (QStringList{QStringLiteral("DPI ativo 7050 fora da lista de estágios (perfil 0)"),
                              QStringLiteral("o mouse marca o perfil 1 como ativo, mas ele está desabilitado; o "
                                             "perfil em uso pode ser outro")}));
    }

    void warningWhenNoActiveProfile()
    {
        runner->setStatus(readyWith({profile(0, false, true)}));
        model->onReadFinished();
        QCOMPARE(model->warnings(), (QStringList{QStringLiteral("nenhum perfil ativo")}));
    }

    void noWarningsForConsistentConfiguration()
    {
        runner->setStatus(readyWith({profile(0, true, true, {stage(0, 800, std::nullopt, true, true, {800, 1600})})}));
        model->onReadFinished();
        QVERIFY(model->warnings().isEmpty());
    }

    void newReadReplacesPreviousResult()
    {
        runner->setStatus(evidenceStatus());
        model->onReadFinished();
        QCOMPARE(model->profiles().size(), 5);

        runner->setStatus(Unavailable{{.code = ErrorCode::BackendUnavailable, .detail = "serviço ausente"}});
        model->onReadFinished();

        QVERIFY(model->profiles().isEmpty());
        QVERIFY(model->warnings().isEmpty());
        QVERIFY(model->deviceName().isEmpty());
        QCOMPARE(model->state(), QStringLiteral("unavailable"));
    }
};

} // namespace

QTEST_GUILESS_MAIN(PointerViewModelTest)
#include "PointerViewModelTest.moc"
