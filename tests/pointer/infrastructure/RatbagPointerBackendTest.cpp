#include "modules/pointer/infrastructure/RatbagPointerBackend.hpp"
#include "FakeRatbagService.hpp"

#include <QDBusConnection>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>

using namespace ghub::fakes;
using namespace ghub::fakes::dbusval;
using ghub::pointer::ButtonActionKind;
using ghub::pointer::DiscoveredPointer;
using ghub::pointer::LedMode;
using ghub::pointer::PointerWarningKind;
using ghub::pointer::RatbagPointerBackend;
using ghub::shared_kernel::ErrorCode;

namespace {

constexpr auto callTimeout = std::chrono::milliseconds{5000};
constexpr QLatin1StringView ifaceDevice{"org.freedesktop.ratbag1.Device"};
constexpr QLatin1StringView ifaceProfile{"org.freedesktop.ratbag1.Profile"};
constexpr QLatin1StringView ifaceResolution{"org.freedesktop.ratbag1.Resolution"};
constexpr QLatin1StringView ifaceButton{"org.freedesktop.ratbag1.Button"};
constexpr QLatin1StringView ifaceLed{"org.freedesktop.ratbag1.Led"};
constexpr QLatin1StringView ifaceManager{"org.freedesktop.ratbag1.Manager"};

QList<uint> g703DpiList()
{
    QList<uint> list;
    for (uint dpi = 100; dpi <= 1000; dpi += 50) {
        list.push_back(dpi);
    }
    for (uint dpi = 1100; dpi <= 2600; dpi += 100) {
        list.push_back(dpi);
    }
    for (uint dpi = 2800; dpi <= 5000; dpi += 200) {
        list.push_back(dpi);
    }
    for (uint dpi = 5500; dpi <= 25500; dpi += 500) {
        list.push_back(dpi);
    }
    return list;
}

QString objectPath(const QString& kind, const QString& name)
{
    return QString(ratbagRoot) + QLatin1Char('/') + kind + QLatin1Char('/') + name;
}

// Adiciona o G703 da evidência (5 perfis; p0 com dados; p1 ativo e desabilitado).
QString addG703(RatbagTree& tree)
{
    const QString device = objectPath(QStringLiteral("device"), QStringLiteral("hidraw8"));
    QStringList profiles;
    for (int p = 0; p < 5; ++p) {
        const QString profilePath = objectPath(QStringLiteral("profile"), QStringLiteral("g703_p%1").arg(p));
        profiles.push_back(profilePath);
        QStringList resolutions;
        QStringList buttons;
        QStringList leds;
        if (p == 0) {
            for (int r = 0; r < 5; ++r) {
                const QString path = objectPath(QStringLiteral("resolution"), QStringLiteral("g703_p0_r%1").arg(r));
                resolutions.push_back(path);
                tree[path][ifaceResolution] = {
                    {QStringLiteral("Index"), static_cast<uint>(r)},
                    {QStringLiteral("IsActive"), r == 0},
                    {QStringLiteral("IsDefault"), r == 0},
                    {QStringLiteral("IsDisabled"), r != 0},
                    {QStringLiteral("Resolution"), variant(r == 0 ? 7050U : 100U)},
                    {QStringLiteral("Resolutions"), uintArray(g703DpiList())},
                };
            }
            for (int b = 0; b < 6; ++b) {
                const QString path = objectPath(QStringLiteral("button"), QStringLiteral("g703_p0_b%1").arg(b));
                buttons.push_back(path);
                const QVariant map = b < 5 ? mapping(1, static_cast<uint>(b + 1)) : mapping(2, 1073741831U);
                tree[path][ifaceButton] = {
                    {QStringLiteral("Index"), static_cast<uint>(b)},
                    {QStringLiteral("Mapping"), map},
                    {QStringLiteral("ActionTypes"), uintArray({0, 1, 2, 3, 4})},
                };
            }
            for (int l = 0; l < 2; ++l) {
                const QString path = objectPath(QStringLiteral("led"), QStringLiteral("g703_p0_l%1").arg(l));
                leds.push_back(path);
                tree[path][ifaceLed] = {
                    {QStringLiteral("Index"), static_cast<uint>(l)},
                    {QStringLiteral("Modes"), uintArray({0, 1, 2, 3})},
                    {QStringLiteral("Mode"), 0U},
                    {QStringLiteral("Color"), uintTriple(0, 0, 0)},
                    {QStringLiteral("ColorDepth"), 1U},
                    {QStringLiteral("EffectDuration"), 0U},
                    {QStringLiteral("Brightness"), 255U},
                };
            }
        }
        tree[profilePath][ifaceProfile] = {
            {QStringLiteral("Index"), static_cast<uint>(p)},
            {QStringLiteral("Name"), QString()},
            {QStringLiteral("Disabled"), p != 0},
            {QStringLiteral("IsActive"), p == 1},
            {QStringLiteral("ReportRate"), p == 0 ? 1000U : 0U},
            {QStringLiteral("ReportRates"), uintArray(p == 0 ? QList<uint>{125, 250, 500, 1000} : QList<uint>{})},
            {QStringLiteral("Resolutions"), pathArray(resolutions)},
            {QStringLiteral("Buttons"), pathArray(buttons)},
            {QStringLiteral("Leds"), pathArray(leds)},
        };
    }
    tree[device][ifaceDevice] = {
        {QStringLiteral("Model"), QStringLiteral("usb:046d:4086:0")},
        {QStringLiteral("Name"), QStringLiteral("Logitech G703 LS")},
        {QStringLiteral("Profiles"), pathArray(profiles)},
    };
    return device;
}

// G733: 1 perfil, sem estágios, botões nem LEDs.
QString addG733(RatbagTree& tree)
{
    const QString device = objectPath(QStringLiteral("device"), QStringLiteral("hidraw3"));
    const QString profilePath = objectPath(QStringLiteral("profile"), QStringLiteral("g733_p0"));
    tree[profilePath][ifaceProfile] = {
        {QStringLiteral("Index"), 0U},
        {QStringLiteral("Name"), QString()},
        {QStringLiteral("Disabled"), false},
        {QStringLiteral("IsActive"), true},
        {QStringLiteral("ReportRate"), 0U},
        {QStringLiteral("ReportRates"), uintArray({})},
        {QStringLiteral("Resolutions"), pathArray({})},
        {QStringLiteral("Buttons"), pathArray({})},
        {QStringLiteral("Leds"), pathArray({})},
    };
    tree[device][ifaceDevice] = {
        {QStringLiteral("Model"), QStringLiteral("usb:046d:0afe:0")},
        {QStringLiteral("Name"), QStringLiteral("Logitech G733 Gaming Headset")},
        {QStringLiteral("Profiles"), pathArray({profilePath})},
    };
    return device;
}

void setManager(RatbagTree& tree, const QStringList& devices, int apiVersion = 2)
{
    tree[ratbagRoot][ifaceManager] = {
        {QStringLiteral("APIVersion"), apiVersion},
        {QStringLiteral("Devices"), pathArray(devices)},
    };
}

RatbagTree evidenceTree()
{
    RatbagTree tree;
    const QString g703 = addG703(tree);
    const QString g733 = addG733(tree);
    setManager(tree, {g733, g703});
    return tree;
}

class RatbagPointerBackendTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { daemon_ = std::make_unique<PrivateDbusDaemon>(); }
    static void TearDownTestSuite() { daemon_.reset(); }

    void SetUp() override
    {
        ASSERT_FALSE(daemon_->address().isEmpty()) << "dbus-daemon privado não iniciou";
        client_ = QDBusConnection::connectToBus(daemon_->address(), QStringLiteral("pointer-test-client"));
        ASSERT_TRUE(client_.isConnected());
    }

    void TearDown() override
    {
        // Garantia de leitura (ADR-0011 §6a): nada além de Properties.Get/GetAll chegou ao serviço.
        if (service_) {
            for (const QString& call : service_->received()) {
                EXPECT_TRUE(call == QLatin1String("org.freedesktop.DBus.Properties.Get") ||
                            call == QLatin1String("org.freedesktop.DBus.Properties.GetAll"))
                    << "chamada inesperada: " << call.toStdString();
            }
        }
        service_.reset();
        client_ = QDBusConnection(QString());
        QDBusConnection::disconnectFromBus(QStringLiteral("pointer-test-client"));
    }

    void startService(RatbagTree tree, bool silent = false)
    {
        service_ = std::make_unique<FakeRatbagService>(daemon_->address(), std::move(tree), silent);
        ASSERT_TRUE(service_->started());
    }

    ghub::shared_kernel::Result<std::vector<DiscoveredPointer>> read(std::chrono::milliseconds timeout = callTimeout)
    {
        return RatbagPointerBackend{client_, ratbagService, timeout}.readDevices();
    }

    static const DiscoveredPointer& deviceNamed(const std::vector<DiscoveredPointer>& devices, const char* name)
    {
        return *std::ranges::find(devices, name, &DiscoveredPointer::name);
    }

    static std::unique_ptr<PrivateDbusDaemon> daemon_;
    QDBusConnection client_{QString()};
    std::unique_ptr<FakeRatbagService> service_;
};

std::unique_ptr<PrivateDbusDaemon> RatbagPointerBackendTest::daemon_;

} // namespace

TEST_F(RatbagPointerBackendTest, ReadsTheEvidenceG703AndG733)
{
    startService(evidenceTree());

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    ASSERT_EQ(devices->size(), 2U);
    const auto& g703 = deviceNamed(*devices, "Logitech G703 LS");
    EXPECT_EQ(g703.usbId, (std::optional<ghub::device_inventory::UsbId>{{.vendorId = 0x046D, .productId = 0x4086}}));
    ASSERT_TRUE(g703.configuration.has_value()) << g703.configuration.error().detail;
    const auto& configuration = *g703.configuration;
    EXPECT_EQ(configuration.name(), "Logitech G703 LS");
    ASSERT_EQ(configuration.profiles().size(), 5U);

    const auto& p0 = configuration.profiles()[0];
    EXPECT_EQ(p0.reportRate(), 1000U);
    EXPECT_EQ(p0.supportedReportRates(), (std::vector<std::uint32_t>{125, 250, 500, 1000}));
    EXPECT_TRUE(p0.isEnabled());
    EXPECT_FALSE(p0.isActive());
    ASSERT_EQ(p0.stages().size(), 5U);
    EXPECT_EQ(p0.stages()[0].dpiX(), 7050U);
    EXPECT_TRUE(p0.stages()[0].isActive());
    EXPECT_TRUE(p0.stages()[0].isDefault());
    EXPECT_FALSE(p0.stages()[0].isDpiListed());
    EXPECT_EQ(p0.stages()[0].supportedDpis().size(), 88U);
    EXPECT_EQ(p0.stages()[1].dpiX(), 100U);
    EXPECT_TRUE(p0.stages()[1].isDisabled());
    ASSERT_EQ(p0.buttons().size(), 6U);
    EXPECT_EQ(p0.buttons()[0].action(), ghub::pointer::ButtonAction::button(1));
    EXPECT_EQ(p0.buttons()[4].action(), ghub::pointer::ButtonAction::button(5));
    EXPECT_EQ(p0.buttons()[5].action(), ghub::pointer::ButtonAction::special(1073741831U));
    EXPECT_EQ(p0.buttons()[5].supportedKinds().size(), 5U);
    ASSERT_EQ(p0.leds().size(), 2U);
    EXPECT_EQ(p0.leds()[0].mode(), LedMode::Off);
    EXPECT_EQ(p0.leds()[1].supportedModes().size(), 4U);

    const auto& p1 = configuration.profiles()[1];
    EXPECT_TRUE(p1.isActive());
    EXPECT_FALSE(p1.isEnabled());
    const auto warnings = configuration.warnings();
    EXPECT_TRUE(std::ranges::any_of(
        warnings, [](const auto& warning) { return warning.kind == PointerWarningKind::ActiveProfileDisabled; }));

    const auto& g733 = deviceNamed(*devices, "Logitech G733 Gaming Headset");
    EXPECT_EQ(g733.usbId, (std::optional<ghub::device_inventory::UsbId>{{.vendorId = 0x046D, .productId = 0x0AFE}}));
    ASSERT_TRUE(g733.configuration.has_value()) << g733.configuration.error().detail;
    EXPECT_EQ(g733.configuration->profiles().size(), 1U);
    EXPECT_TRUE(g733.configuration->profiles()[0].buttons().empty());
    EXPECT_TRUE(g733.configuration->profiles()[0].leds().empty());
}

TEST_F(RatbagPointerBackendTest, MissingServiceIsBackendUnavailable)
{
    const auto devices = read();

    ASSERT_FALSE(devices.has_value());
    EXPECT_EQ(devices.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(devices.error().detail.find("ServiceUnknown"), std::string::npos);
}

TEST_F(RatbagPointerBackendTest, UnsupportedApiVersionStopsAfterTheVersionCall)
{
    RatbagTree tree = evidenceTree();
    setManager(tree, {objectPath(QStringLiteral("device"), QStringLiteral("hidraw8"))}, 1);
    startService(tree);

    const auto devices = read();

    ASSERT_FALSE(devices.has_value());
    EXPECT_EQ(devices.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(service_->received(), QStringList{QStringLiteral("org.freedesktop.DBus.Properties.Get")});
}

TEST_F(RatbagPointerBackendTest, ServiceThatNeverRepliesTimesOut)
{
    startService(evidenceTree(), true);

    const auto devices = read(std::chrono::milliseconds{300});

    ASSERT_FALSE(devices.has_value());
    EXPECT_EQ(devices.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(devices.error().detail.find("NoReply"), std::string::npos);
}

TEST_F(RatbagPointerBackendTest, ReadsSeparateXAndYResolution)
{
    RatbagTree tree = evidenceTree();
    tree[objectPath(QStringLiteral("resolution"), QStringLiteral("g703_p0_r0"))][ifaceResolution]
        [QStringLiteral("Resolution")] = variant(uintPair(3200, 1600));
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    const auto& configuration = deviceNamed(*devices, "Logitech G703 LS").configuration;
    ASSERT_TRUE(configuration.has_value()) << configuration.error().detail;
    const auto& stage = configuration->profiles()[0].stages()[0];
    EXPECT_EQ(stage.dpiX(), 3200U);
    EXPECT_EQ(stage.dpiY(), std::optional<std::uint32_t>{1600});
}

TEST_F(RatbagPointerBackendTest, ReadsMacroAsStepCount)
{
    RatbagTree tree = evidenceTree();
    tree[objectPath(QStringLiteral("button"), QStringLiteral("g703_p0_b2"))][ifaceButton][QStringLiteral("Mapping")] =
        mapping(4, macroSteps(3));
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    const auto& configuration = deviceNamed(*devices, "Logitech G703 LS").configuration;
    ASSERT_TRUE(configuration.has_value()) << configuration.error().detail;
    EXPECT_EQ(configuration->profiles()[0].buttons()[2].action(), ghub::pointer::ButtonAction::macro(3));
}

TEST_F(RatbagPointerBackendTest, MacroWithMoreThan1024StepsIsMalformedForThatDevice)
{
    RatbagTree tree = evidenceTree();
    tree[objectPath(QStringLiteral("button"), QStringLiteral("g703_p0_b2"))][ifaceButton][QStringLiteral("Mapping")] =
        mapping(4, macroSteps(1025));
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    const auto& configuration = deviceNamed(*devices, "Logitech G703 LS").configuration;
    ASSERT_FALSE(configuration.has_value());
    EXPECT_EQ(configuration.error().code, ErrorCode::MalformedData);
}

TEST_F(RatbagPointerBackendTest, ReadsKeyNoneAndUnknownButtonTypes)
{
    RatbagTree tree = evidenceTree();
    const auto set = [&tree](int b, const QVariant& map) {
        tree[objectPath(QStringLiteral("button"), QStringLiteral("g703_p0_b%1").arg(b))][ifaceButton]
            [QStringLiteral("Mapping")] = map;
    };
    set(0, mapping(0, 0U));
    set(1, mapping(3, 30U));
    set(2, mapping(1000, 0U));
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    const auto& buttons = deviceNamed(*devices, "Logitech G703 LS").configuration->profiles()[0].buttons();
    EXPECT_EQ(buttons[0].action(), ghub::pointer::ButtonAction::none());
    EXPECT_EQ(buttons[1].action(), ghub::pointer::ButtonAction::key(30));
    EXPECT_EQ(buttons[2].action().kind(), ButtonActionKind::Unknown);
}

TEST_F(RatbagPointerBackendTest, UnknownLedModeKeepsTheRawValue)
{
    RatbagTree tree = evidenceTree();
    tree[objectPath(QStringLiteral("led"), QStringLiteral("g703_p0_l1"))][ifaceLed][QStringLiteral("Mode")] = 9U;
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    const auto& led = deviceNamed(*devices, "Logitech G703 LS").configuration->profiles()[0].leds()[1];
    EXPECT_EQ(led.mode(), LedMode::Unknown);
    EXPECT_EQ(led.rawMode(), 9U);
}

TEST_F(RatbagPointerBackendTest, WrongPropertyTypeIsMalformedOnlyForThatDevice)
{
    RatbagTree tree = evidenceTree();
    tree[objectPath(QStringLiteral("profile"), QStringLiteral("g703_p0"))][ifaceProfile][QStringLiteral("ReportRate")] =
        QStringLiteral("1000");
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    const auto& g703 = deviceNamed(*devices, "Logitech G703 LS");
    ASSERT_FALSE(g703.configuration.has_value());
    EXPECT_EQ(g703.configuration.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(deviceNamed(*devices, "Logitech G733 Gaming Headset").configuration.has_value());
}

TEST_F(RatbagPointerBackendTest, InvalidDomainValueIsMalformedForThatDevice)
{
    RatbagTree tree = evidenceTree();
    tree[objectPath(QStringLiteral("led"), QStringLiteral("g703_p0_l0"))][ifaceLed][QStringLiteral("Color")] =
        uintTriple(300, 0, 0);
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    const auto& g703 = deviceNamed(*devices, "Logitech G703 LS");
    ASSERT_FALSE(g703.configuration.has_value());
    EXPECT_EQ(g703.configuration.error().code, ErrorCode::MalformedData);
}

TEST_F(RatbagPointerBackendTest, NonUsbModelHasNoUsbId)
{
    RatbagTree tree = evidenceTree();
    tree[objectPath(QStringLiteral("device"), QStringLiteral("hidraw8"))][ifaceDevice][QStringLiteral("Model")] =
        QStringLiteral("bluetooth:046d:b02a:0");
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    EXPECT_FALSE(deviceNamed(*devices, "Logitech G703 LS").usbId.has_value());
}

TEST_F(RatbagPointerBackendTest, CableModelParsesTheProductId)
{
    RatbagTree tree = evidenceTree();
    tree[objectPath(QStringLiteral("device"), QStringLiteral("hidraw8"))][ifaceDevice][QStringLiteral("Model")] =
        QStringLiteral("usb:046d:c090:0");
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    EXPECT_EQ(deviceNamed(*devices, "Logitech G703 LS").usbId,
              (std::optional<ghub::device_inventory::UsbId>{{.vendorId = 0x046D, .productId = 0xC090}}));
}

TEST_F(RatbagPointerBackendTest, MalformedUsbModelHasNoUsbId)
{
    RatbagTree tree = evidenceTree();
    tree[objectPath(QStringLiteral("device"), QStringLiteral("hidraw8"))][ifaceDevice][QStringLiteral("Model")] =
        QStringLiteral("usb:zzzz:4086:0");
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    EXPECT_FALSE(deviceNamed(*devices, "Logitech G703 LS").usbId.has_value());
}

TEST_F(RatbagPointerBackendTest, MoreThan16ProfilesIsMalformedForThatDevice)
{
    RatbagTree tree = evidenceTree();
    const QString device = objectPath(QStringLiteral("device"), QStringLiteral("hidraw8"));
    QStringList profiles;
    for (int i = 0; i < 17; ++i) {
        profiles.push_back(objectPath(QStringLiteral("profile"), QStringLiteral("g733_p0")));
    }
    tree[device][ifaceDevice][QStringLiteral("Profiles")] = pathArray(profiles);
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    const auto& g703 = deviceNamed(*devices, "Logitech G703 LS");
    ASSERT_FALSE(g703.configuration.has_value());
    EXPECT_EQ(g703.configuration.error().code, ErrorCode::MalformedData);
    EXPECT_TRUE(deviceNamed(*devices, "Logitech G733 Gaming Headset").configuration.has_value());
}

TEST_F(RatbagPointerBackendTest, MoreThan64ButtonsIsMalformedForThatDevice)
{
    RatbagTree tree = evidenceTree();
    QStringList buttons;
    for (int i = 0; i < 65; ++i) {
        buttons.push_back(objectPath(QStringLiteral("button"), QStringLiteral("g703_p0_b0")));
    }
    tree[objectPath(QStringLiteral("profile"), QStringLiteral("g703_p0"))][ifaceProfile][QStringLiteral("Buttons")] =
        pathArray(buttons);
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    EXPECT_EQ(deviceNamed(*devices, "Logitech G703 LS").configuration.error().code, ErrorCode::MalformedData);
}

TEST_F(RatbagPointerBackendTest, MoreThan1024ItemsInAListIsMalformedForThatDevice)
{
    RatbagTree tree = evidenceTree();
    QList<uint> rates;
    for (uint i = 1; i <= 1025; ++i) {
        rates.push_back(i);
    }
    tree[objectPath(QStringLiteral("profile"), QStringLiteral("g703_p0"))][ifaceProfile]
        [QStringLiteral("ReportRates")] = uintArray(rates);
    startService(tree);

    const auto devices = read();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    EXPECT_EQ(deviceNamed(*devices, "Logitech G703 LS").configuration.error().code, ErrorCode::MalformedData);
}

TEST_F(RatbagPointerBackendTest, MoreThan16DevicesIsAGlobalMalformedError)
{
    RatbagTree tree = evidenceTree();
    QStringList devices;
    for (int i = 0; i < 17; ++i) {
        devices.push_back(objectPath(QStringLiteral("device"), QStringLiteral("hidraw3")));
    }
    setManager(tree, devices);
    startService(tree);

    const auto result = read();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MalformedData);
}

TEST_F(RatbagPointerBackendTest, ObjectErrorFromTheServiceIsBackendUnavailable)
{
    RatbagTree tree = evidenceTree();
    setManager(tree, {objectPath(QStringLiteral("device"), QStringLiteral("inexistente"))});
    startService(tree);

    const auto result = read();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_NE(result.error().detail.find("UnknownObject"), std::string::npos);
}

TEST_F(RatbagPointerBackendTest, SendsOnlyPropertiesGetAndGetAllForTheWholeTree)
{
    startService(evidenceTree());

    ASSERT_TRUE(read().has_value());

    const QStringList calls = service_->received();
    EXPECT_GT(calls.size(), 20);
    // Um cliente nosso nunca pergunta pela estrutura (Introspect) nem escreve (Set/Commit/SetActive).
    EXPECT_FALSE(calls.contains(QStringLiteral("org.freedesktop.DBus.Introspectable.Introspect")));
    EXPECT_FALSE(calls.contains(QStringLiteral("org.freedesktop.DBus.Properties.Set")));
    EXPECT_FALSE(calls.contains(QStringLiteral("org.freedesktop.ratbag1.Device.Commit")));
}
