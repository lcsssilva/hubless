// KWinActiveApplicationSource contra um KWin falso num dbus-daemon privado (nunca o KWin nem a sessão reais).
#include "FakeKWinService.hpp"

#include "modules/profiles/infrastructure/KWinActiveApplicationSource.hpp"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <optional>
#include <string>
#include <vector>

using ghub::fakes::FakeKWinBus;
using ghub::fakes::FakeKWinService;
using ghub::profiles::KWinActiveApplicationSource;
using ghub::shared_kernel::DomainError;
using ghub::shared_kernel::ErrorCode;

namespace {
const QString ownService = QStringLiteral("io.github.hubless.GHubLinux");
const QString plugin = QStringLiteral("ghub-linux-active-window");
const QString kwinServiceName = QStringLiteral("org.kde.KWin");
} // namespace

class KWinActiveApplicationSourceTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        bus_ = std::make_unique<FakeKWinBus>();
        QVERIFY(!bus_->address().isEmpty());
    }

    void cleanupTestCase() { bus_.reset(); }

    void init()
    {
        QVERIFY(runtime_.isValid());
        activations_.clear();
        statuses_.clear();
        sourceConn_ = QDBusConnection::connectToBus(bus_->address(), QStringLiteral("test-source"));
        clientConn_ = QDBusConnection::connectToBus(bus_->address(), QStringLiteral("test-client"));
        QVERIFY(sourceConn_.isConnected());
        QVERIFY(clientConn_.isConnected());
    }

    void cleanup()
    {
        sourceConn_ = QDBusConnection(QString());
        clientConn_ = QDBusConnection(QString());
        QDBusConnection::disconnectFromBus(QStringLiteral("test-source"));
        QDBusConnection::disconnectFromBus(QStringLiteral("test-client"));
    }

    void startCallsUnloadThenLoadThenRunInOrder()
    {
        FakeKWinService kwin(bus_->address());
        QVERIFY(kwin.started());
        kwin.setLoadResult(7);
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        const QString script = runtime_.filePath(QStringLiteral("ghub-linux/active-window.js"));
        QCOMPARE(kwin.calls(), (QStringList{QStringLiteral("unloadScript|") + plugin,
                                            QStringLiteral("loadScript|") + script + QLatin1Char('|') + plugin,
                                            QStringLiteral("run|/Scripting/Script7")}));
    }

    void startWritesConstantScriptWithRestrictedPermissionsAndNoCaption()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        const QFileInfo dir(runtime_.filePath(QStringLiteral("ghub-linux")));
        const QFileInfo file(runtime_.filePath(QStringLiteral("ghub-linux/active-window.js")));
        QVERIFY(dir.isDir());
        QCOMPARE(dir.permissions() & (QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                      QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup |
                                      QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther),
                 QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        QCOMPARE(file.permissions() & (QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                       QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup |
                                       QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther),
                 QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QFile f(file.filePath());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray content = f.readAll();
        QVERIFY(content.contains("windowActivated"));
        QVERIFY(content.contains("io.github.hubless.GHubLinux"));
        QVERIFY(!content.contains("caption"));
    }

    void startNeverCallsScriptingStart()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        for (const QString& call : kwin.calls()) {
            QVERIFY2(!call.contains(QLatin1String("start")), qPrintable(call));
        }
    }

    void startOverwritesAnExistingScriptFile()
    {
        QVERIFY(QDir(runtime_.path()).mkpath(QStringLiteral("ghub-linux")));
        QFile old(runtime_.filePath(QStringLiteral("ghub-linux/active-window.js")));
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write("lixo antigo");
        old.close();
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        QVERIFY(old.open(QIODevice::ReadOnly));
        QVERIFY(old.readAll().contains("windowActivated"));
    }

    void startTwiceIsSuccessWithoutNewCalls()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        const QStringList before = kwin.calls();
        QVERIFY(source.start(listener(), status()).has_value());
        QCOMPARE(kwin.calls(), before);
    }

    void emptyRuntimeDirFailsWithoutTouchingTheBus()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, QString());
        const auto result = source.start(listener(), status());
        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::BackendUnavailable);
        QVERIFY(kwin.calls().isEmpty());
        QVERIFY(!clientConn_.interface()->isServiceRegistered(ownService));
    }

    void loadScriptRefusedFailsAndReleasesTheServiceName()
    {
        FakeKWinService kwin(bus_->address());
        kwin.setLoadResult(-1);
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        const auto result = source.start(listener(), status());
        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::BackendUnavailable);
        QVERIFY(!result.error().detail.empty());
        QVERIFY(!clientConn_.interface()->isServiceRegistered(ownService));
        for (const QString& call : kwin.calls()) {
            QVERIFY2(!call.startsWith(QLatin1String("run")), qPrintable(call));
        }
    }

    void missingKWinFailsAndReleasesTheServiceName()
    {
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        const auto result = source.start(listener(), status());
        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::BackendUnavailable);
        QVERIFY(!clientConn_.interface()->isServiceRegistered(ownService));
    }

    void deadlineExpiredFailsAndReleasesTheServiceName()
    {
        FakeKWinService kwin(bus_->address());
        kwin.setDelayMs(600);
        KWinActiveApplicationSource source(sourceConn_, runtime_.path(), std::chrono::milliseconds(150));
        const auto result = source.start(listener(), status());
        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::BackendUnavailable);
        QVERIFY(!clientConn_.interface()->isServiceRegistered(ownService));
    }

    void activatedPrefersDesktopFileNameAndNormalizesCase()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        sendActivated(QStringLiteral("Firefox"), QStringLiteral("Org.Mozilla.Firefox"));
        QTRY_COMPARE(activations_.size(), std::size_t{1});
        QCOMPARE(activations_.front(), std::string("org.mozilla.firefox"));
    }

    void activatedFallsBackToResourceClassWhenDesktopFileNameIsInvalidOrEmpty()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        sendActivated(QStringLiteral("Steam"), QString());
        sendActivated(QStringLiteral("Kate"), QStringLiteral("nome inválido/com espaço"));
        QTRY_COMPARE(activations_.size(), std::size_t{2});
        QCOMPARE(activations_.at(0), std::string("steam"));
        QCOMPARE(activations_.at(1), std::string("kate"));
    }

    void activatedInvalidOrOversizedIsDiscarded()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        sendActivated(QStringLiteral("a b"), QStringLiteral("c/d"));
        sendActivated(QString(300, QLatin1Char('a')), QString(300, QLatin1Char('b')));
        sendActivated(QString(), QString());
        // Sentinela válida: se chegou, as anteriores já foram processadas (mesma ordem de entrega).
        sendActivated(QStringLiteral("ok"), QString());
        QTRY_COMPARE(activations_.size(), std::size_t{1});
        QCOMPARE(activations_.front(), std::string("ok"));
    }

    void activatedOversizedDesktopFileNameFallsBackToValidResourceClass()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        sendActivated(QStringLiteral("vlc"), QString(300, QLatin1Char('x')));
        sendActivated(QStringLiteral("sentinela"), QString());
        QTRY_COMPARE(activations_.size(), std::size_t{2});
        QCOMPARE(activations_.at(0), std::string("vlc"));
    }

    void stopUnloadsTheScriptAndReleasesTheServiceName()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        QVERIFY(clientConn_.interface()->isServiceRegistered(ownService));
        kwin.clearCalls();
        source.stop();
        QCOMPARE(kwin.calls(), (QStringList{QStringLiteral("unloadScript|") + plugin}));
        QVERIFY(!clientConn_.interface()->isServiceRegistered(ownService));
        source.stop(); // idempotente
        QCOMPARE(kwin.calls().size(), 1);
    }

    void activatedAfterStopIsNotDelivered()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        source.stop();
        sendActivated(QStringLiteral("late"), QString());
        drainBus(); // sincronização positiva: tudo o que já estava a caminho foi entregue e processado
        QVERIFY(activations_.empty());
    }

    void stopThenStartAgainWorks()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        source.stop();
        QVERIFY(source.start(listener(), status()).has_value());
        sendActivated(QStringLiteral("again"), QString());
        QTRY_COMPARE(activations_.size(), std::size_t{1});
    }

    void destructorUnloadsTheScriptAndReleasesTheServiceName()
    {
        FakeKWinService kwin(bus_->address());
        {
            KWinActiveApplicationSource source(sourceConn_, runtime_.path());
            QVERIFY(source.start(listener(), status()).has_value());
            kwin.clearCalls();
        }
        QCOMPARE(kwin.calls(), (QStringList{QStringLiteral("unloadScript|") + plugin}));
        QVERIFY(!clientConn_.interface()->isServiceRegistered(ownService));
    }

    void destructorOfANeverStartedSourceCallsNothing()
    {
        FakeKWinService kwin(bus_->address());
        {
            KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        }
        QVERIFY(kwin.calls().isEmpty());
    }

    void kwinLeavingReportsErrorAndReturningReloadsTheScriptOnce()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        kwin.clearCalls();

        kwin.disappear();
        QTRY_COMPARE(statuses_.size(), std::size_t{1});
        QVERIFY(statuses_.front().has_value());
        QCOMPARE(statuses_.front()->code, ErrorCode::BackendUnavailable);

        kwin.reappear();
        QTRY_COMPARE(statuses_.size(), std::size_t{2});
        QVERIFY(!statuses_.back().has_value());
        const QString script = runtime_.filePath(QStringLiteral("ghub-linux/active-window.js"));
        QCOMPARE(kwin.calls(), (QStringList{QStringLiteral("unloadScript|") + plugin,
                                            QStringLiteral("loadScript|") + script + QLatin1Char('|') + plugin,
                                            QStringLiteral("run|/Scripting/Script7")}));
    }

    void reloadFailureAfterKWinReturnsReportsError()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        kwin.disappear();
        QTRY_COMPARE(statuses_.size(), std::size_t{1});
        kwin.setLoadResult(-1);
        kwin.reappear();
        QTRY_COMPARE(statuses_.size(), std::size_t{2});
        QVERIFY(statuses_.back().has_value());
    }

    void stoppedSourceIgnoresKWinRestart()
    {
        FakeKWinService kwin(bus_->address());
        KWinActiveApplicationSource source(sourceConn_, runtime_.path());
        QVERIFY(source.start(listener(), status()).has_value());
        source.stop();
        kwin.clearCalls();
        kwin.disappear();
        kwin.reappear();
        drainBus(); // sincronização positiva: os avisos de dono do nome já chegaram à fonte parada
        QVERIFY(kwin.calls().isEmpty());
        QVERIFY(statuses_.empty());
    }

private:
    // Garante que tudo o que foi enviado antes já foi entregue e processado, sem esperar por tempo: uma chamada
    // bloqueante de ida e volta em cada conexão (o dbus-daemon atende na ordem e o socket é um fluxo; a resposta
    // só chega depois das mensagens anteriores) e, depois, o processamento dos eventos enfileirados.
    void drainBus()
    {
        QVERIFY(clientConn_.interface()->isServiceRegistered(kwinServiceName).isValid());
        QVERIFY(sourceConn_.interface()->isServiceRegistered(kwinServiceName).isValid());
        QCoreApplication::processEvents();
    }

    [[nodiscard]] KWinActiveApplicationSource::ActivationListener listener()
    {
        return [this](ghub::profiles::ApplicationId id) { activations_.push_back(id.value()); };
    }
    [[nodiscard]] KWinActiveApplicationSource::StatusListener status()
    {
        return [this](std::optional<DomainError> e) { statuses_.push_back(std::move(e)); };
    }
    void sendActivated(const QString& resourceClass, const QString& desktopFileName)
    {
        auto message = QDBusMessage::createMethodCall(ownService, QStringLiteral("/ActiveWindow"),
                                                      QStringLiteral("io.github.hubless.ActiveWindow"),
                                                      QStringLiteral("Activated"));
        message << resourceClass << desktopFileName;
        QVERIFY(clientConn_.send(message));
    }

    std::unique_ptr<FakeKWinBus> bus_;
    QTemporaryDir runtime_;
    QDBusConnection sourceConn_{QString()};
    QDBusConnection clientConn_{QString()};
    std::vector<std::string> activations_;
    std::vector<std::optional<DomainError>> statuses_;
};

QTEST_MAIN(KWinActiveApplicationSourceTest)
#include "KWinActiveApplicationSourceTest.moc"
