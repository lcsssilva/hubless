#include "DesktopController.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

using ghub::app::DesktopController;

namespace {

const QString kDesktopName = QStringLiteral("ghub-linux.desktop");

// Escreve um arquivo à mão para simular estado deixado por fora do app (só nos testes).
bool writeFile(const QString& path, const QByteArray& content)
{
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(content) == content.size();
}

QByteArray bytesOf(const QString& text)
{
    return text.toUtf8();
}

} // namespace

class DesktopControllerTest : public QObject {
    Q_OBJECT

private slots:
    void withoutTrayClosingEndsTheApp()
    {
        const DesktopController controller{false, true};

        QVERIFY(!controller.trayAvailable());
        QVERIFY(!controller.hideOnClose());
        QVERIFY(!controller.startHidden()); // sem bandeja não há como esconder: --autostart cai no normal
    }

    void startHiddenNeedsBothTrayAndRequest()
    {
        const DesktopController requested{true, true};
        QVERIFY(requested.trayAvailable());
        QVERIFY(requested.hideOnClose());
        QVERIFY(requested.startHidden());

        const DesktopController notRequested{true, false};
        QVERIFY(!notRequested.startHidden());

        const DesktopController defaultRequest{true};
        QVERIFY(!defaultRequest.startHidden());
    }

    void showWindowAsksTheWindowToShow()
    {
        DesktopController controller{true};
        QSignalSpy spy{&controller, &DesktopController::showWindowRequested};

        controller.showWindow();

        QCOMPARE(spy.count(), 1);
    }

    void requestQuitMarksQuittingAndAsksOnlyOnce()
    {
        DesktopController controller{true};
        QSignalSpy quittingSpy{&controller, &DesktopController::quittingChanged};
        QSignalSpy quitSpy{&controller, &DesktopController::quitRequested};
        QVERIFY(!controller.quitting());

        controller.requestQuit();

        QVERIFY(controller.quitting());
        QCOMPARE(quittingSpy.count(), 1);
        QCOMPARE(quitSpy.count(), 1);

        controller.requestQuit(); // já encerrando: ignorado

        QVERIFY(controller.quitting());
        QCOMPARE(quittingSpy.count(), 1);
        QCOMPARE(quitSpy.count(), 1);
    }

    void enablingAutostartWritesTheFileAndPublishesState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DesktopController controller{true, false, directory.path()};
        QVERIFY(!controller.autostartEnabled());
        const QString statusBefore = controller.status();
        QVERIFY(!statusBefore.isEmpty());
        QSignalSpy autostartSpy{&controller, &DesktopController::autostartChanged};
        QSignalSpy statusSpy{&controller, &DesktopController::statusChanged};

        controller.setAutostart(true);

        QVERIFY(controller.autostartEnabled());
        QVERIFY(QFile::exists(directory.filePath(kDesktopName)));
        QVERIFY(!controller.status().isEmpty());
        QCOMPARE(autostartSpy.count(), 1);
        QCOMPARE(statusSpy.count(), 1);

        controller.setAutostart(false);

        QVERIFY(!controller.autostartEnabled());
        QVERIFY(!QFile::exists(directory.filePath(kDesktopName)));
        QCOMPARE(autostartSpy.count(), 2);
        QCOMPARE(statusSpy.count(), 2);
    }

    void autostartFailureKeepsStateAndPublishesTheReason()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString commonFile = directory.filePath(QStringLiteral("arquivo-comum"));
        QVERIFY(writeFile(commonFile, bytesOf(QStringLiteral("não é diretório\n"))));
        DesktopController controller{true, false, commonFile}; // diretório inutilizável
        const QString statusBefore = controller.status();
        QSignalSpy autostartSpy{&controller, &DesktopController::autostartChanged};
        QSignalSpy statusSpy{&controller, &DesktopController::statusChanged};

        controller.setAutostart(true);

        QVERIFY(!controller.autostartEnabled()); // estado inalterado: a janela não pode mentir
        QVERIFY(!controller.status().isEmpty());
        QVERIFY(controller.status() != statusBefore); // o motivo da falha aparece no status
        QCOMPARE(autostartSpy.count(), 0);
        QCOMPARE(statusSpy.count(), 1);
    }

    void refreshAutostartRereadsAnExternallyWrittenFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DesktopController controller{true, false, directory.path()};
        QVERIFY(!controller.autostartEnabled());
        QVERIFY(writeFile(directory.filePath(kDesktopName),
                          bytesOf(QStringLiteral("[Desktop Entry]\nType=Application\nHidden=false\n"))));
        QSignalSpy spy{&controller, &DesktopController::autostartChanged};

        controller.refreshAutostart();

        QVERIFY(controller.autostartEnabled());
        QCOMPARE(spy.count(), 1);
    }

    void refreshAutostartKeepsHiddenFileDisabled()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(writeFile(directory.filePath(kDesktopName), bytesOf(QStringLiteral("[Desktop Entry]\nHidden=true\n"))));
        DesktopController controller{true, false, directory.path()};

        controller.refreshAutostart();

        QVERIFY(!controller.autostartEnabled());
    }

    void constructionWritesNothing()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        {
            const DesktopController controller{true, false, directory.path()};
            QVERIFY(!controller.autostartEnabled());
        }

        QVERIFY(QDir{directory.path()}.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
        QVERIFY(!QFile::exists(directory.filePath(kDesktopName)));
    }
};

QTEST_GUILESS_MAIN(DesktopControllerTest)
#include "DesktopControllerTest.moc"
