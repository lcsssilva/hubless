#include "modules/diagnostics/application/RenderDiagnosticReport.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

using ghub::diagnostics::BackendHealth;
using ghub::diagnostics::DiagnosticEntry;
using ghub::diagnostics::DiagnosticReportInput;
using ghub::diagnostics::DiagnosticSeverity;
using ghub::diagnostics::renderDiagnosticReport;

namespace {

::testing::AssertionResult contains(const std::string& text, const std::string& needle)
{
    if (text.find(needle) != std::string::npos) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << "'" << needle << "' não aparece no relatório";
}

std::size_t position(const std::string& text, const std::string& needle)
{
    return text.find(needle);
}

DiagnosticReportInput reportInput()
{
    return DiagnosticReportInput{
        .generatedAt = "2026-10-14T10:00:00.000-03:00",
        .appVersion = "0.0.1",
        .platform = "linux",
        .backends =
            {
                BackendHealth{.name = "Bandeja", .available = true, .detail = "KDE Plasma"},
                BackendHealth{.name = "ratbagd", .available = false, .detail = "serviço inativo"},
            },
        .entries =
            {
                DiagnosticEntry{.timestamp = "2026-10-14T09:59:59.000-03:00",
                                .severity = DiagnosticSeverity::Warning,
                                .category = "ghub.lighting",
                                .message = "G915 sem resposta"},
                DiagnosticEntry{.timestamp = "2026-10-14T09:58:00.000-03:00",
                                .severity = DiagnosticSeverity::Info,
                                .category = "ghub.battery",
                                .message = "bateria lida"},
            },
    };
}

} // namespace

TEST(RenderDiagnosticReport, IncludesGenerationInstantVersionAndPlatform)
{
    const auto text = renderDiagnosticReport(reportInput());

    EXPECT_TRUE(contains(text, "2026-10-14T10:00:00.000-03:00"));
    EXPECT_TRUE(contains(text, "0.0.1"));
    EXPECT_TRUE(contains(text, "linux"));
}

TEST(RenderDiagnosticReport, StartsWithTheProductName)
{
    const auto text = renderDiagnosticReport(reportInput());

    EXPECT_TRUE(text.starts_with("Relatório de diagnóstico do Hubless\n"));
}

TEST(RenderDiagnosticReport, ListsEveryBackendWithAvailabilityAndDetail)
{
    const auto text = renderDiagnosticReport(reportInput());

    EXPECT_TRUE(contains(text, "Bandeja — ok — KDE Plasma"));
    EXPECT_TRUE(contains(text, "ratbagd — indisponível — serviço inativo"));
}

TEST(RenderDiagnosticReport, ListsMessagesWithInstantSeverityCategoryAndText)
{
    const auto text = renderDiagnosticReport(reportInput());

    EXPECT_TRUE(contains(text, "2026-10-14T09:59:59.000-03:00 [Warning] ghub.lighting: G915 sem resposta"));
    EXPECT_TRUE(contains(text, "2026-10-14T09:58:00.000-03:00 [Info] ghub.battery: bateria lida"));
}

TEST(RenderDiagnosticReport, KeepsMostRecentMessageFirst)
{
    const auto text = renderDiagnosticReport(reportInput());
    const auto recent = position(text, "G915 sem resposta");
    const auto older = position(text, "bateria lida");

    ASSERT_NE(recent, std::string::npos);
    ASSERT_NE(older, std::string::npos);
    EXPECT_LT(recent, older);
}

TEST(RenderDiagnosticReport, EndsWithNewline)
{
    const auto text = renderDiagnosticReport(reportInput());

    ASSERT_FALSE(text.empty());
    EXPECT_EQ(text.back(), '\n');
}

TEST(RenderDiagnosticReport, RendersHeaderWhenThereIsNoBackendOrEntry)
{
    const DiagnosticReportInput empty{
        .generatedAt = "2026-10-14T10:00:00.000-03:00",
        .appVersion = "0.0.1",
        .platform = "linux",
        .backends = {},
        .entries = {},
    };

    const auto text = renderDiagnosticReport(empty);

    ASSERT_FALSE(text.empty());
    EXPECT_TRUE(contains(text, "0.0.1"));
    EXPECT_TRUE(contains(text, "linux"));
    EXPECT_EQ(text.back(), '\n');
}
