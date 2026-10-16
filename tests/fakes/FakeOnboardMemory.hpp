#pragma once

#include "modules/onboard_memory/application/OnboardBackupStore.hpp"
#include "modules/onboard_memory/application/OnboardMemory.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ghub::fakes {

// Estado compartilhado entre o conector e as memórias que ele abre: setores, ordem das chamadas e falhas
// injetadas. writeSector guarda os bytes, salvo se `postWriteOverride` estiver definido (simula mouse que
// devolve dados diferentes na releitura).
struct FakeOnboardState {
    onboard_memory::OnboardLayout layout{.sectorSize = 255, .profileCount = 5};
    std::uint8_t mode = 1;
    std::uint16_t currentSector = 1;
    std::map<std::uint16_t, std::vector<std::uint8_t>> sectors;

    std::optional<shared_kernel::DomainError> openError;
    std::optional<shared_kernel::DomainError> describeError;
    std::optional<shared_kernel::DomainError> writeError;
    std::optional<std::vector<std::uint8_t>> postWriteOverride;

    std::vector<std::string> calls;
    int opens = 0;
    int liveMemories = 0;

    [[nodiscard]] int count(const std::string& call) const { return static_cast<int>(std::ranges::count(calls, call)); }
};

class FakeOnboardMemory final : public onboard_memory::OnboardMemory {
public:
    explicit FakeOnboardMemory(FakeOnboardState& state) : state_(&state) { ++state_->liveMemories; }
    FakeOnboardMemory(const FakeOnboardMemory&) = delete;
    FakeOnboardMemory& operator=(const FakeOnboardMemory&) = delete;
    ~FakeOnboardMemory() override { --state_->liveMemories; }

    shared_kernel::Result<onboard_memory::OnboardLayout> describe() override
    {
        state_->calls.emplace_back("describe");
        if (state_->describeError) {
            return std::unexpected(*state_->describeError);
        }
        return state_->layout;
    }
    shared_kernel::Result<std::uint8_t> onboardMode() override
    {
        state_->calls.emplace_back("mode");
        return state_->mode;
    }
    shared_kernel::Result<std::uint16_t> currentProfileSector() override
    {
        state_->calls.emplace_back("current");
        return state_->currentSector;
    }
    shared_kernel::Result<std::vector<std::uint8_t>> readSector(std::uint16_t sector, std::uint16_t /*size*/) override
    {
        state_->calls.push_back("read:" + std::to_string(sector));
        const auto found = state_->sectors.find(sector);
        if (found == state_->sectors.end()) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "setor ausente no fake");
        }
        return found->second;
    }
    shared_kernel::Result<void> writeSector(std::uint16_t sector, std::span<const std::uint8_t> bytes) override
    {
        state_->calls.push_back("write:" + std::to_string(sector));
        if (state_->writeError) {
            return std::unexpected(*state_->writeError);
        }
        state_->sectors[sector] =
            state_->postWriteOverride.value_or(std::vector<std::uint8_t>{bytes.begin(), bytes.end()});
        return {};
    }

private:
    FakeOnboardState* state_;
};

class FakeOnboardMemoryConnector final : public onboard_memory::OnboardMemoryConnector {
public:
    shared_kernel::Result<std::unique_ptr<onboard_memory::OnboardMemory>> open() override
    {
        ++state.opens;
        state.calls.emplace_back("open");
        if (state.openError) {
            return std::unexpected(*state.openError);
        }
        return std::make_unique<FakeOnboardMemory>(state);
    }

    FakeOnboardState state;
};

// Cópias em memória. `saveIfAbsent` registra a chamada; se já houver cópia, mantém a antiga.
class FakeOnboardBackupStore final : public onboard_memory::OnboardBackupStore {
public:
    shared_kernel::Result<std::optional<std::vector<std::uint8_t>>> load(std::uint16_t sector) override
    {
        if (loadError) {
            return std::unexpected(*loadError);
        }
        const auto found = stored.find(sector);
        if (found == stored.end()) {
            return std::optional<std::vector<std::uint8_t>>{};
        }
        return std::optional<std::vector<std::uint8_t>>{found->second};
    }
    shared_kernel::Result<void> saveIfAbsent(std::uint16_t sector, std::span<const std::uint8_t> bytes) override
    {
        ++saveCalls;
        if (log != nullptr) {
            log->push_back("backup:" + std::to_string(sector));
        }
        if (onSave) {
            onSave();
        }
        if (saveError) {
            return std::unexpected(*saveError);
        }
        stored.try_emplace(sector, std::vector<std::uint8_t>{bytes.begin(), bytes.end()});
        return {};
    }

    std::map<std::uint16_t, std::vector<std::uint8_t>> stored;
    std::optional<shared_kernel::DomainError> loadError;
    std::optional<shared_kernel::DomainError> saveError;
    int saveCalls = 0;
    std::function<void()> onSave;            // gancho do teste (ex.: avançar o relógio entre o backup e a escrita)
    std::vector<std::string>* log = nullptr; // se definido, registra "backup:<setor>" na ordem das demais chamadas
};

} // namespace ghub::fakes
