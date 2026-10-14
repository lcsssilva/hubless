#include "modules/diagnostics/presentation/DiagnosticsViewModel.hpp"
#include "modules/diagnostics/application/DiagnosticLog.hpp"
#include "modules/diagnostics/application/ExportDiagnosticReport.hpp"

#include <QSignalSpy>
#include <QTest>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using ghub::diagnostics::BackendHealth;
using ghub::diagnostics::DiagnosticEntry;
using ghub::diagnostics::DiagnosticLog;
using ghub::diagnostics::DiagnosticSeverity;
using ghub::diagnostics::DiagnosticsViewModel;
using ghub::diagnostics::ExportDiagnosticReport;
using ghub::diagnostics::ReportWriter;
using ghub::shared_kernel::ErrorCode;
using ghub::shared_kernel::Result;

namespace {

// Port falso: conta as chamadas, guarda o conteúdo recebido e devolve o resultado programado.
class FakeReportWriter final : public ReportWriter {
public:
    explicit FakeReportWriter(Result<std::string> result) : result_(std::move(result)) {}

    [[nodiscard]] Result<std::string> write(std::string_view content) const override
    {
        ++calls_;
        content_ = std::string(content);
        return result_;
    }

    [[nodiscard]] int calls() const { return calls_; }
    [[nodiscard]] const std::string& receivedContent() const { return content_; }

private:
    Result<std::string> result_;
    mutable int calls_ = 0;
    mutable std::string content_;
};

QVariantMap item(const QVariantList& list, qsizetype index)
{
    return list.at(index).toMap();
}

} // namespace

class DiagnosticsViewModelTest : public QObject {
    Q_OBJECT

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` é `private:` para o clang.
private slots:
    void publishesBackendsAndEntriesFromThePorts()
    {
        health = {
            BackendHealth{.name = "Bandeja", .available = true, .detail = "KDE Plasma"},
            BackendHealth{.name = "ratbagd", .available = false, .detail = "serviço inativo"},
        };
        log.record(entry("primeira", DiagnosticSeverity::Warning));
        log.record(entry("segunda", DiagnosticSeverity::Info));
        DiagnosticsViewModel viewModel{log, exportReport, "0.0.1", "linux", [this] { return health; }};
        QSignalSpy changed{&viewModel, &DiagnosticsViewModel::changed};

        viewModel.refresh();

        QCOMPARE(changed.count(), 1);
        QCOMPARE(viewModel.backends().size(), qsizetype{2});
        QCOMPARE(item(viewModel.backends(), 0).value("name").toString(), QStringLiteral("Bandeja"));
        QCOMPARE(item(viewModel.backends(), 0).value("available").toBool(), true);
        QCOMPARE(item(viewModel.backends(), 0).value("detail").toString(), QStringLiteral("KDE Plasma"));
        QCOMPARE(item(viewModel.backends(), 1).value("name").toString(), QStringLiteral("ratbagd"));
        QCOMPARE(item(viewModel.backends(), 1).value("available").toBool(), false);
        QCOMPARE(item(viewModel.backends(), 1).value("detail").toString(), QStringLiteral("serviço inativo"));
        // Mais recente primeiro, com severidade pelo nome.
        QCOMPARE(viewModel.entries().size(), qsizetype{2});
        QCOMPARE(item(viewModel.entries(), 0).value("message").toString(), QStringLiteral("segunda"));
        QCOMPARE(item(viewModel.entries(), 0).value("severity").toString(), QStringLiteral("Info"));
        QCOMPARE(item(viewModel.entries(), 0).value("category").toString(), QStringLiteral("ghub.teste"));
        QCOMPARE(item(viewModel.entries(), 0).value("timestamp").toString(),
                 QStringLiteral("2026-10-14T09:59:59.000-03:00"));
        QCOMPARE(item(viewModel.entries(), 1).value("message").toString(), QStringLiteral("primeira"));
        QCOMPARE(item(viewModel.entries(), 1).value("severity").toString(), QStringLiteral("Warning"));
    }

    void exportsNothingBeforeTheCommand()
    {
        health = {BackendHealth{.name = "Bandeja", .available = true, .detail = "KDE Plasma"}};
        log.record(entry("mensagem", DiagnosticSeverity::Info));
        DiagnosticsViewModel viewModel{log, exportReport, "0.0.1", "linux", [this] { return health; }};

        viewModel.refresh();

        QCOMPARE(writer.calls(), 0);
        QVERIFY(viewModel.lastReportPath().isEmpty());
        QVERIFY(viewModel.status().isEmpty());
    }

    void exportsTheDisplayedReportAndPublishesThePath()
    {
        health = {BackendHealth{.name = "HeadsetControl", .available = true, .detail = "4.1.0"}};
        log.record(entry("bateria lida", DiagnosticSeverity::Info));
        DiagnosticsViewModel viewModel{log, exportReport, "0.0.1", "linux", [this] { return health; }};
        viewModel.refresh();
        QSignalSpy changed{&viewModel, &DiagnosticsViewModel::changed};

        viewModel.exportReport();

        QCOMPARE(writer.calls(), 1);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(viewModel.lastReportPath(), QStringLiteral("/tmp/diagnostico.txt"));
        QVERIFY(viewModel.status().contains(QStringLiteral("/tmp/diagnostico.txt")));
        const auto content = QString::fromStdString(writer.receivedContent());
        QVERIFY(content.contains(QStringLiteral("0.0.1")));
        QVERIFY(content.contains(QStringLiteral("linux")));
        QVERIFY(content.contains(QStringLiteral("HeadsetControl")));
        QVERIFY(content.contains(QStringLiteral("bateria lida")));
    }

    void publishesTheWriterErrorInStatus()
    {
        FakeReportWriter failing{ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "sem espaço")};
        const ExportDiagnosticReport failingExport{failing};
        log.record(entry("mensagem", DiagnosticSeverity::Critical));
        DiagnosticsViewModel viewModel{log, failingExport, "0.0.1", "linux", [this] { return health; }};
        viewModel.refresh();

        viewModel.exportReport();

        QCOMPARE(failing.calls(), 1);
        QVERIFY(viewModel.lastReportPath().isEmpty());
        QVERIFY(viewModel.status().contains(QStringLiteral("sem espaço")));
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    DiagnosticEntry entry(std::string message, DiagnosticSeverity severity)
    {
        return DiagnosticEntry{
            .timestamp = "2026-10-14T09:59:59.000-03:00",
            .severity = severity,
            .category = "ghub.teste",
            .message = std::move(message),
        };
    }

    std::vector<BackendHealth> health;
    DiagnosticLog log;
    FakeReportWriter writer{std::string{"/tmp/diagnostico.txt"}};
    ExportDiagnosticReport exportReport{writer};
};

QTEST_GUILESS_MAIN(DiagnosticsViewModelTest)
#include "DiagnosticsViewModelTest.moc"
