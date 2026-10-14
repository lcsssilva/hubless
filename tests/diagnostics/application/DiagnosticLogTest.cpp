#include "modules/diagnostics/application/DiagnosticLog.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

using ghub::diagnostics::DiagnosticEntry;
using ghub::diagnostics::DiagnosticLog;
using ghub::diagnostics::DiagnosticSeverity;

namespace {

DiagnosticEntry entry(std::string message, DiagnosticSeverity severity = DiagnosticSeverity::Info)
{
    return DiagnosticEntry{
        .timestamp = "2026-10-14T10:00:00.000-03:00",
        .severity = severity,
        .category = "teste",
        .message = std::move(message),
    };
}

std::vector<std::string> messagesOf(const std::vector<DiagnosticEntry>& entries)
{
    std::vector<std::string> messages;
    messages.reserve(entries.size());
    for (const auto& item : entries) {
        messages.push_back(item.message);
    }
    return messages;
}

} // namespace

TEST(DiagnosticLog, IsEmptyWhenNothingWasRecorded)
{
    const DiagnosticLog log;

    EXPECT_EQ(log.size(), 0U);
    EXPECT_TRUE(log.snapshot().empty());
}

TEST(DiagnosticLog, KeepsTheMostRecentEntriesUpToCapacity)
{
    DiagnosticLog log{3};
    for (int i = 0; i < 5; ++i) {
        log.record(entry("mensagem " + std::to_string(i)));
    }

    EXPECT_EQ(log.size(), 3U);
    EXPECT_EQ(messagesOf(log.snapshot()), (std::vector<std::string>{"mensagem 4", "mensagem 3", "mensagem 2"}));
}

TEST(DiagnosticLog, DefaultCapacityKeepsTheLastFiveHundred)
{
    DiagnosticLog log;
    for (int i = 0; i < 600; ++i) {
        log.record(entry(std::to_string(i)));
    }

    EXPECT_EQ(log.size(), 500U);
    const auto messages = messagesOf(log.snapshot());
    ASSERT_EQ(messages.size(), 500U);
    EXPECT_EQ(messages.front(), "599");
    EXPECT_EQ(messages.back(), "100");
}

TEST(DiagnosticLog, PreservesTimestampSeverityCategoryAndMessage)
{
    DiagnosticLog log{1};

    log.record(DiagnosticEntry{
        .timestamp = "2026-10-14T09:59:59.000-03:00",
        .severity = DiagnosticSeverity::Warning,
        .category = "ghub.teste",
        .message = "sem resposta",
    });

    const auto entries = log.snapshot();
    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries[0].timestamp, "2026-10-14T09:59:59.000-03:00");
    EXPECT_EQ(entries[0].severity, DiagnosticSeverity::Warning);
    EXPECT_EQ(entries[0].category, "ghub.teste");
    EXPECT_EQ(entries[0].message, "sem resposta");
}

TEST(DiagnosticLog, ClearEmptiesTheBuffer)
{
    DiagnosticLog log{2};
    log.record(entry("a"));
    log.record(entry("b"));

    log.clear();

    EXPECT_EQ(log.size(), 0U);
    EXPECT_TRUE(log.snapshot().empty());
    log.record(entry("c"));
    EXPECT_EQ(messagesOf(log.snapshot()), (std::vector<std::string>{"c"}));
}

TEST(DiagnosticLog, HandlesConcurrentWritersWithoutLosingOrTearingEntries)
{
    constexpr std::size_t perThread = 500;
    DiagnosticLog log{2 * perThread};
    const auto write = [&log](const std::string& prefix) {
        for (std::size_t i = 0; i < perThread; ++i) {
            log.record(entry(prefix + std::to_string(i), DiagnosticSeverity::Debug));
        }
    };

    std::thread first{write, "a-"};
    std::thread second{write, "b-"};
    first.join();
    second.join();

    EXPECT_EQ(log.size(), 2 * perThread);
    auto messages = messagesOf(log.snapshot());
    ASSERT_EQ(messages.size(), 2 * perThread);
    std::ranges::sort(messages);
    // Nenhuma entrada perdida nem duplicada: as 2 * perThread mensagens aparecem uma única vez.
    EXPECT_EQ(std::ranges::adjacent_find(messages), messages.end());
    for (std::size_t i = 0; i < perThread; ++i) {
        EXPECT_TRUE(std::ranges::binary_search(messages, "a-" + std::to_string(i)));
        EXPECT_TRUE(std::ranges::binary_search(messages, "b-" + std::to_string(i)));
    }
}
