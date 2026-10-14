#include "modules/diagnostics/application/ExportDiagnosticReport.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <utility>

using ghub::diagnostics::DiagnosticEntry;
using ghub::diagnostics::DiagnosticReportInput;
using ghub::diagnostics::DiagnosticSeverity;
using ghub::diagnostics::ExportDiagnosticReport;
using ghub::diagnostics::ReportWriter;
using ghub::shared_kernel::ErrorCode;
using ghub::shared_kernel::Result;

namespace {

// Port falso: guarda o conteúdo recebido, conta as chamadas e devolve o resultado programado.
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

DiagnosticReportInput reportInput()
{
    return DiagnosticReportInput{
        .generatedAt = "2026-10-14T10:00:00.000-03:00",
        .appVersion = "0.0.1",
        .platform = "linux",
        .backends = {},
        .entries =
            {
                DiagnosticEntry{.timestamp = "2026-10-14T09:59:59.000-03:00",
                                .severity = DiagnosticSeverity::Critical,
                                .category = "ghub.camera",
                                .message = "v4l2 falhou"},
            },
    };
}

} // namespace

TEST(ExportDiagnosticReport, ReturnsThePathWrittenByThePort)
{
    const FakeReportWriter writer{std::string{"/tmp/diagnostico-20261014-100000.txt"}};
    const ExportDiagnosticReport exportReport{writer};

    const auto result = exportReport.run(reportInput());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/diagnostico-20261014-100000.txt");
}

TEST(ExportDiagnosticReport, WritesTheRenderedReport)
{
    const FakeReportWriter writer{std::string{"/tmp/diagnostico.txt"}};
    const ExportDiagnosticReport exportReport{writer};

    ASSERT_TRUE(exportReport.run(reportInput()).has_value());

    EXPECT_EQ(writer.calls(), 1);
    EXPECT_NE(writer.receivedContent().find("0.0.1"), std::string::npos);
    EXPECT_NE(writer.receivedContent().find("linux"), std::string::npos);
    EXPECT_NE(writer.receivedContent().find("v4l2 falhou"), std::string::npos);
    EXPECT_EQ(writer.receivedContent().back(), '\n');
}

TEST(ExportDiagnosticReport, PropagatesTheWriterFailure)
{
    const FakeReportWriter writer{ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "disco cheio")};
    const ExportDiagnosticReport exportReport{writer};

    const auto result = exportReport.run(reportInput());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(result.error().detail, "disco cheio");
    EXPECT_EQ(writer.calls(), 1);
}

TEST(ExportDiagnosticReport, WritesNothingBeforeRunIsCalled)
{
    const FakeReportWriter writer{std::string{"/tmp/diagnostico.txt"}};
    const ExportDiagnosticReport exportReport{writer};

    EXPECT_EQ(writer.calls(), 0);
    EXPECT_TRUE(writer.receivedContent().empty());
}
