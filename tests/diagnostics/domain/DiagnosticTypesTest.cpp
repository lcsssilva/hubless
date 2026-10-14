#include "modules/diagnostics/domain/DiagnosticTypes.hpp"

#include <gtest/gtest.h>

using ghub::diagnostics::DiagnosticSeverity;
using ghub::diagnostics::severityName;

TEST(DiagnosticTypes, NamesEverySeverity)
{
    EXPECT_EQ(severityName(DiagnosticSeverity::Debug), "Debug");
    EXPECT_EQ(severityName(DiagnosticSeverity::Info), "Info");
    EXPECT_EQ(severityName(DiagnosticSeverity::Warning), "Warning");
    EXPECT_EQ(severityName(DiagnosticSeverity::Critical), "Critical");
}
