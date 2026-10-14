#include "AutostartEntry.hpp"
#include "AutostartManager.hpp"
#include "shared_kernel/DomainError.hpp"

#include <QChar>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

using ghub::app::AutostartEntry;
using ghub::app::AutostartManager;
using ghub::app::defaultAutostartEntry;
using ghub::app::renderAutostartEntry;

namespace {

// Escreve um arquivo à mão para simular conteúdo de fora do app (só nos testes).
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

class AutostartTest : public QObject {
    Q_OBJECT

private slots:
    void defaultEntryRendersEveryRequiredKeyInOrder()
    {
        const AutostartEntry entry = defaultAutostartEntry();
        const QString text = renderAutostartEntry(entry);

        QVERIFY(text.startsWith(QStringLiteral("[Desktop Entry]\n")));
        QVERIFY(text.contains(QStringLiteral("Type=Application\n")));
        QVERIFY(text.contains(QStringLiteral("Version=1.0\n")));
        QVERIFY(text.contains(QStringLiteral("Name=G HUB para Linux\n")));
        QVERIFY(text.contains(QStringLiteral("\nComment=") + entry.comment + QLatin1Char('\n')));
        QVERIFY(text.contains(QStringLiteral("Exec=ghub-linux --autostart\n")));
        QVERIFY(text.contains(QStringLiteral("Icon=ghub-linux\n")));
        QVERIFY(text.contains(QStringLiteral("Terminal=false\n")));
        QVERIFY(text.contains(QStringLiteral("StartupNotify=false\n")));
        QVERIFY(text.contains(QStringLiteral("Categories=Utility;\n")));
        QVERIFY(text.contains(QStringLiteral("Hidden=false\n")));
        QVERIFY(text.contains(QStringLiteral("X-GNOME-Autostart-enabled=true\n")));

        // A ordem da spec importa: a entrada é lida por ferramentas que esperam o cabeçalho primeiro.
        const QStringList keysInOrder{"Type=Application",
                                      "Version=1.0",
                                      "Name=",
                                      "Comment=",
                                      "Exec=",
                                      "Icon=",
                                      "Terminal=false",
                                      "StartupNotify=false",
                                      "Categories=Utility;",
                                      "Hidden=false",
                                      "X-GNOME-Autostart-enabled=true"};
        qsizetype previous = -1;
        for (const QString& key : keysInOrder) {
            const qsizetype position = text.indexOf(key);
            QVERIFY2(position > previous,
                     qPrintable(QStringLiteral("%1 fora de ordem (posição %2)").arg(key).arg(position)));
            previous = position;
        }

        // A última linha termina em quebra simples (spec: o arquivo não pode ficar sem '\n' final).
        QVERIFY(text.endsWith(QLatin1Char('\n')));
        QVERIFY(!text.endsWith(QStringLiteral("\n\n")));
    }

    void renderUsesTheGivenEntryValues()
    {
        const AutostartEntry entry{.name = QStringLiteral("Outro nome"),
                                   .comment = QStringLiteral("Outro comentário"),
                                   .exec = QStringLiteral("/caminho/absoluto/ghub-linux --autostart"),
                                   .icon = QStringLiteral("outro-icone")};
        const QString text = renderAutostartEntry(entry);

        QVERIFY(text.contains(QStringLiteral("Name=Outro nome\n")));
        QVERIFY(text.contains(QStringLiteral("Comment=Outro comentário\n")));
        QVERIFY(text.contains(QStringLiteral("Exec=/caminho/absoluto/ghub-linux --autostart\n")));
        QVERIFY(text.contains(QStringLiteral("Icon=outro-icone\n")));
        // Os valores vêm da entrada recebida: o padrão do app não vaza no que foi pedido.
        QVERIFY(!text.contains(QStringLiteral("Exec=ghub-linux --autostart\n")));
        QVERIFY(!text.contains(QStringLiteral("Name=G HUB para Linux\n")));
    }

    void defaultDirectoryIsTheXdgAutostartDirectory()
    {
        const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        QVERIFY2(!config.isEmpty(), "ambiente de teste sem XDG_CONFIG_HOME nem HOME");

        const AutostartManager manager;

        QCOMPARE(manager.filePath(), config + QStringLiteral("/autostart/ghub-linux.desktop"));
    }

    void disabledWhenFileIsAbsent()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const AutostartManager manager{directory.path()};

        QCOMPARE(manager.filePath(), directory.filePath(QStringLiteral("ghub-linux.desktop")));
        QVERIFY(!QFile::exists(manager.filePath()));
        QVERIFY(!manager.isEnabled());
    }

    void enablingWritesTheRenderedEntry()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const AutostartManager manager{directory.path()};

        const auto result = manager.setEnabled(true);

        QVERIFY2(result.has_value(), result.has_value() ? "" : result.error().detail.c_str());
        QVERIFY(QFile::exists(manager.filePath()));
        QVERIFY(manager.isEnabled());
        QFile file{manager.filePath()};
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(file.readAll()), renderAutostartEntry(defaultAutostartEntry()));
    }

    void enablingWhenAlreadyEnabledIsStillSuccess()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const AutostartManager manager{directory.path()};
        QVERIFY(manager.setEnabled(true).has_value());

        const auto result = manager.setEnabled(true);

        QVERIFY(result.has_value());
        QVERIFY(manager.isEnabled());
    }

    void disablingRemovesTheFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const AutostartManager manager{directory.path()};
        QVERIFY(manager.setEnabled(true).has_value());
        QVERIFY(manager.isEnabled());

        const auto result = manager.setEnabled(false);

        QVERIFY(result.has_value());
        QVERIFY(!QFile::exists(manager.filePath()));
        QVERIFY(!manager.isEnabled());
    }

    void disablingWhenAbsentIsStillSuccess()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const AutostartManager manager{directory.path()};

        QVERIFY(manager.setEnabled(false).has_value());
        QVERIFY(manager.setEnabled(false).has_value());
        QVERIFY(!manager.isEnabled());
    }

    void missingDirectoryIsCreated()
    {
        QTemporaryDir parent;
        QVERIFY(parent.isValid());
        const QString nested = parent.filePath(QStringLiteral("autostart"));
        const AutostartManager manager{nested};
        QVERIFY(!QDir{nested}.exists());

        const auto result = manager.setEnabled(true);

        QVERIFY(result.has_value());
        QVERIFY(QDir{nested}.exists());
        QVERIFY(QFile::exists(manager.filePath()));
        QVERIFY(manager.isEnabled());
    }

    void hiddenTrueCountsAsDisabled()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const AutostartManager manager{directory.path()};

        QVERIFY(writeFile(manager.filePath(), bytesOf(QStringLiteral("[Desktop Entry]\nHidden=true\n"))));
        QVERIFY(!manager.isEnabled());

        QVERIFY(writeFile(manager.filePath(), bytesOf(QStringLiteral("[Desktop Entry]\nHidden = TRUE \n"))));
        QVERIFY(!manager.isEnabled());

        QVERIFY(writeFile(manager.filePath(), bytesOf(QStringLiteral("[Desktop Entry]\nHidden=false\n"))));
        QVERIFY(manager.isEnabled());
    }

    void writeFailureBecomesBackendUnavailable()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString commonFile = directory.filePath(QStringLiteral("arquivo-comum"));
        QVERIFY(writeFile(commonFile, bytesOf(QStringLiteral("não é diretório\n"))));

        // Diretório apontando para um arquivo comum: nada pode ser criado nem gravado a partir dele.
        const AutostartManager manager{commonFile};

        const auto result = manager.setEnabled(true);

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ghub::shared_kernel::ErrorCode::BackendUnavailable);
        QVERIFY(!result.error().detail.empty());
    }
};

QTEST_GUILESS_MAIN(AutostartTest)
#include "AutostartTest.moc"
