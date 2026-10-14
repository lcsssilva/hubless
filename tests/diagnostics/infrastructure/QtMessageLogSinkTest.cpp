#include "modules/diagnostics/infrastructure/QtMessageLogSink.hpp"
#include "modules/diagnostics/application/DiagnosticLog.hpp"

#include <QDateTime>
#include <QLoggingCategory>
#include <QString>
#include <QStringList>
#include <QTest>

#include <cstddef>
#include <string>
#include <vector>

using ghub::diagnostics::DiagnosticEntry;
using ghub::diagnostics::DiagnosticLog;
using ghub::diagnostics::DiagnosticSeverity;
using ghub::diagnostics::QtMessageLogSink;

Q_LOGGING_CATEGORY(diagnosticsTestCategory, "ghub.teste.diagnostics", QtDebugMsg)

namespace {

// Sonda instalada antes do sink: prova o encadeamento e a restauração do tratador anterior.
QStringList probeMessages;
int probeCalls = 0;

void probeHandler(QtMsgType /*type*/, const QMessageLogContext& /*context*/, const QString& message)
{
    ++probeCalls;
    probeMessages.append(message);
}

// Entradas da categoria do teste, na ordem em que o log as expõe (mais recentes primeiro).
std::vector<DiagnosticEntry> entriesOfCategory(const DiagnosticLog& log, const QString& category)
{
    std::vector<DiagnosticEntry> entries;
    for (const auto& entry : log.snapshot()) {
        if (QString::fromStdString(entry.category) == category) {
            entries.push_back(entry);
        }
    }
    return entries;
}

} // namespace

class QtMessageLogSinkTest : public QObject {
    Q_OBJECT

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` é `private:` para o clang.
private slots:
    void init()
    {
        probeMessages.clear();
        probeCalls = 0;
        previousHandler_ = qInstallMessageHandler(&probeHandler);
    }

    void cleanup() { qInstallMessageHandler(previousHandler_); }

    void mapsEverySeverityAndUsesTheCategory()
    {
        DiagnosticLog log;
        {
            const QtMessageLogSink sink{log};
            qCDebug(diagnosticsTestCategory) << "depuração";
            qCInfo(diagnosticsTestCategory) << "informação";
            qCWarning(diagnosticsTestCategory) << "atenção";
            qCCritical(diagnosticsTestCategory) << "falha";
        }

        const auto entries = entriesOfCategory(log, QStringLiteral("ghub.teste.diagnostics"));
        QCOMPARE(entries.size(), std::size_t{4});
        QCOMPARE(QString::fromStdString(entries[0].message), QStringLiteral("falha"));
        QVERIFY(entries[0].severity == DiagnosticSeverity::Critical);
        QCOMPARE(QString::fromStdString(entries[1].message), QStringLiteral("atenção"));
        QVERIFY(entries[1].severity == DiagnosticSeverity::Warning);
        QCOMPARE(QString::fromStdString(entries[2].message), QStringLiteral("informação"));
        QVERIFY(entries[2].severity == DiagnosticSeverity::Info);
        QCOMPARE(QString::fromStdString(entries[3].message), QStringLiteral("depuração"));
        QVERIFY(entries[3].severity == DiagnosticSeverity::Debug);
    }

    void stampsEntriesWithIsoDateAndMilliseconds()
    {
        DiagnosticLog log;
        {
            const QtMessageLogSink sink{log};
            qCWarning(diagnosticsTestCategory) << "com instante";
        }

        const auto entries = entriesOfCategory(log, QStringLiteral("ghub.teste.diagnostics"));
        QCOMPARE(entries.size(), std::size_t{1});
        const auto parsed = QDateTime::fromString(QString::fromStdString(entries[0].timestamp), Qt::ISODateWithMs);
        QVERIFY(parsed.isValid());
        const auto age = parsed.secsTo(QDateTime::currentDateTime());
        QVERIFY(age >= 0);
        QVERIFY(age < 60);
    }

    void chainsToThePreviousHandler()
    {
        DiagnosticLog log;
        {
            const QtMessageLogSink sink{log};
            qCWarning(diagnosticsTestCategory) << "encadeada";
        }

        QVERIFY(probeCalls >= 1);
        QVERIFY(probeMessages.contains(QStringLiteral("encadeada")));
        QCOMPARE(entriesOfCategory(log, QStringLiteral("ghub.teste.diagnostics")).size(), std::size_t{1});
    }

    void restoresThePreviousHandlerOnDestruction()
    {
        DiagnosticLog log;
        {
            const QtMessageLogSink sink{log};
            qCWarning(diagnosticsTestCategory) << "com o sink instalado";
        }

        qCWarning(diagnosticsTestCategory) << "depois do sink";

        QVERIFY(probeMessages.contains(QStringLiteral("com o sink instalado")));
        QVERIFY(probeMessages.contains(QStringLiteral("depois do sink")));
        // Depois da destruição nada mais é registrado: o tratador anterior voltou.
        const auto entries = entriesOfCategory(log, QStringLiteral("ghub.teste.diagnostics"));
        QCOMPARE(entries.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(entries[0].message), QStringLiteral("com o sink instalado"));
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    QtMessageHandler previousHandler_ = nullptr;
};

QTEST_GUILESS_MAIN(QtMessageLogSinkTest)
#include "QtMessageLogSinkTest.moc"
