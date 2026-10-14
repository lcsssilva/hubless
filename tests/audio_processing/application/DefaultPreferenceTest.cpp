#include "modules/audio_processing/application/DefaultPreference.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

namespace {

namespace shared_kernel = ghub::shared_kernel;
using ghub::audio_processing::restoreDefaultIfStillOurs;
using Preference = std::optional<std::string>;

struct FakePreference {
    Preference current{};
    std::optional<shared_kernel::DomainError> readError{};
    std::optional<shared_kernel::DomainError> writeError{};
    std::vector<Preference> writes{};

    shared_kernel::Result<Preference> read() const
    {
        if (readError) {
            return std::unexpected(*readError);
        }
        return current;
    }

    shared_kernel::Result<void> write(const Preference& value)
    {
        writes.push_back(value);
        if (writeError) {
            return std::unexpected(*writeError);
        }
        current = value;
        return {};
    }

    shared_kernel::Result<void> restore(const std::string& ours, const Preference& saved)
    {
        return restoreDefaultIfStillOurs([this] { return read(); }, [this](const Preference& v) { return write(v); },
                                         ours, saved);
    }
};

shared_kernel::DomainError error(std::string detail)
{
    return {.code = shared_kernel::ErrorCode::BackendUnavailable, .detail = std::move(detail)};
}

TEST(DefaultPreference, StillOurs_WritesSavedValue)
{
    FakePreference pref{.current = "ghub.eq"};

    ASSERT_TRUE(pref.restore("ghub.eq", "speakers").has_value());

    EXPECT_EQ(pref.writes, std::vector<Preference>{"speakers"});
    EXPECT_EQ(pref.current, "speakers");
}

TEST(DefaultPreference, StillOurs_SavedAbsent_ErasesKey)
{
    FakePreference pref{.current = "ghub.eq"};

    ASSERT_TRUE(pref.restore("ghub.eq", std::nullopt).has_value());

    EXPECT_EQ(pref.writes, std::vector<Preference>{std::nullopt});
    EXPECT_EQ(pref.current, std::nullopt);
}

TEST(DefaultPreference, UserChangedIt_DoesNotWrite)
{
    FakePreference pref{.current = "other"};

    EXPECT_TRUE(pref.restore("ghub.eq", "speakers").has_value());

    EXPECT_TRUE(pref.writes.empty());
    EXPECT_EQ(pref.current, "other");
}

TEST(DefaultPreference, CurrentAbsent_DoesNotWrite)
{
    FakePreference pref{};

    EXPECT_TRUE(pref.restore("ghub.eq", "speakers").has_value());

    EXPECT_TRUE(pref.writes.empty());
}

TEST(DefaultPreference, ReadFails_ReturnsErrorWithoutWriting)
{
    FakePreference pref{.current = "ghub.eq", .readError = error("leitura")};

    const auto result = pref.restore("ghub.eq", "speakers");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "leitura");
    EXPECT_TRUE(pref.writes.empty());
}

TEST(DefaultPreference, WriteFails_ReturnsWriteError)
{
    FakePreference pref{.current = "ghub.eq", .writeError = error("escrita")};

    const auto result = pref.restore("ghub.eq", "speakers");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "escrita");
}

} // namespace
