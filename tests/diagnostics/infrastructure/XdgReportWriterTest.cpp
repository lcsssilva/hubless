#include "modules/diagnostics/infrastructure/XdgReportWriter.hpp"
#include "modules/diagnostics/application/ExportDiagnosticReport.hpp"
#include "modules/diagnostics/application/RenderDiagnosticReport.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QRegularExpression>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

#include <string>

using ghub::diagnostics::BackendHealth;
using ghub::diagnostics::DiagnosticEntry;
using ghub::diagnostics::DiagnosticReportInput;
using ghub::diagnostics::DiagnosticSeverity;
using ghub::diagnostics::ExportDiagnosticReport;
using ghub::diagnostics::renderDiagnosticReport;
using ghub::diagnostics::XdgReportWriter;
using ghub::shared_kernel::ErrorCode;

namespace {

QString readFile(const QString& path)
{
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

class XdgReportWriterTest : public QObject {
    Q_OBJECT

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` é `private:` para o clang.
private slots:
    void writesTheContentAndReturnsThePath()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const XdgReportWriter writer{dir.path()};

        const auto result = writer.write("relatório de diagnóstico\n");

        QVERIFY(result.has_value());
        const auto path = QString::fromStdString(*result);
        QVERIFY(QFile::exists(path));
        QVERIFY(path.startsWith(dir.path()));
        QCOMPARE(readFile(path), QStringLiteral("relatório de diagnóstico\n"));
    }

    void createsTheMissingDirectories()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto nested = dir.filePath(QStringLiteral("a/b/c"));
        const XdgReportWriter writer{nested};

        const auto result = writer.write("x\n");

        QVERIFY(result.has_value());
        QVERIFY(QDir{nested}.exists());
        QVERIFY(QFile::exists(QString::fromStdString(*result)));
    }

    void namesTheFileWithTheDiagnosticTimestamp()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const XdgReportWriter writer{dir.path()};

        const auto result = writer.write("x\n");

        QVERIFY(result.has_value());
        const auto name = QFileInfo{QString::fromStdString(*result)}.fileName();
        const QRegularExpression pattern{QStringLiteral("^diagnostico-\\d{8}-\\d{6}\\.txt$")};
        QVERIFY(pattern.match(name).hasMatch());
    }

    void reportsBackendUnavailableWhenTheDirectoryCannotBeCreated()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Um arquivo comum no lugar do diretório impede a criação.
        QFile blocker{dir.filePath(QStringLiteral("bloqueio"))};
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.write("x");
        blocker.close();
        const XdgReportWriter writer{dir.filePath(QStringLiteral("bloqueio/sub"))};

        const auto result = writer.write("x\n");

        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::BackendUnavailable);
        // O detalhe aponta a origem real da falha (criação do diretório), não um erro genérico.
        QVERIFY(QString::fromStdString(result.error().detail).contains(QStringLiteral("criar")));
    }

    void writesTheReportRenderedByTheUseCase()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const XdgReportWriter writer{dir.path()};
        const ExportDiagnosticReport exportReport{writer};
        const DiagnosticReportInput input{
            .generatedAt = "2026-10-14T10:00:00.000-03:00",
            .appVersion = "0.0.1",
            .platform = "linux",
            .backends = {BackendHealth{.name = "Bandeja", .available = false, .detail = "sem bandeja"}},
            .entries = {DiagnosticEntry{.timestamp = "2026-10-14T09:59:59.000-03:00",
                                        .severity = DiagnosticSeverity::Warning,
                                        .category = "ghub.teste",
                                        .message = "atenção"}},
        };

        const auto result = exportReport.run(input);

        QVERIFY(result.has_value());
        QCOMPARE(readFile(QString::fromStdString(*result)), QString::fromStdString(renderDiagnosticReport(input)));
    }

    void usesTheStateDirectoryByDefault()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        qputenv("XDG_STATE_HOME", dir.path().toUtf8());

        const XdgReportWriter writer;
        const auto result = writer.write("x\n");

        qunsetenv("XDG_STATE_HOME");
        QVERIFY(result.has_value());
        const auto path = QString::fromStdString(*result);
        QVERIFY(path.startsWith(dir.path()));
        QVERIFY(path.contains(QStringLiteral("ghub-linux/diagnosticos/")));
    }
};

QTEST_GUILESS_MAIN(XdgReportWriterTest)
#include "XdgReportWriterTest.moc"
