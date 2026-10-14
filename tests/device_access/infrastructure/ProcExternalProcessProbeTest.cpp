#include "modules/device_access/infrastructure/ProcExternalProcessProbe.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ghub::device_access {
namespace {

namespace fs = std::filesystem;

// /proc falso em diretório temporário RAII.
class FakeProc {
public:
    FakeProc()
    {
        std::string pattern = (fs::temp_directory_path() / "ghub-proc-XXXXXX").string();
        if (::mkdtemp(pattern.data()) == nullptr) {
            throw std::runtime_error("mkdtemp falhou");
        }
        root_ = pattern;
    }
    FakeProc(const FakeProc&) = delete;
    FakeProc& operator=(const FakeProc&) = delete;
    FakeProc(FakeProc&&) = delete;
    FakeProc& operator=(FakeProc&&) = delete;
    ~FakeProc()
    {
        std::error_code ignored;
        fs::remove_all(root_, ignored);
    }

    [[nodiscard]] const fs::path& root() const { return root_; }

    void addProcess(const std::string& dir, const std::string& comm) const
    {
        fs::create_directories(root_ / dir);
        std::ofstream(root_ / dir / "comm", std::ios::binary) << comm;
    }

    void addProcessWithoutComm(const std::string& dir) const { fs::create_directories(root_ / dir); }

private:
    fs::path root_;
};

using Owners = std::vector<ExternalOwner>;

TEST(ProcExternalProcessProbe, DetectsKnownOwners)
{
    const FakeProc proc;
    proc.addProcess("1", "openrgb\n");
    proc.addProcess("22", "bash\n");
    proc.addProcess("30", "solaar\n");
    proc.addProcess("31", "openrgb\n");
    proc.addProcess("self", "ratbagd\n");
    proc.addProcess("abc", "piper\n");

    ProcExternalProcessProbe probe(proc.root());
    const auto owners = probe.runningOwners();

    ASSERT_TRUE(owners.has_value());
    EXPECT_EQ(*owners, (Owners{ExternalOwner::OpenRgb, ExternalOwner::Solaar}));
}

TEST(ProcExternalProcessProbe, IgnoresVanishedProcesses)
{
    const FakeProc proc;
    proc.addProcessWithoutComm("40");
    proc.addProcess("41", "piper\n");

    ProcExternalProcessProbe probe(proc.root());
    const auto owners = probe.runningOwners();

    ASSERT_TRUE(owners.has_value());
    EXPECT_EQ(*owners, (Owners{ExternalOwner::Piper}));
}

TEST(ProcExternalProcessProbe, FailsUnavailable_WhenRootMissing)
{
    const FakeProc proc;
    const auto missing = proc.root() / "inexistente";

    ProcExternalProcessProbe probe(missing);
    const auto owners = probe.runningOwners();

    ASSERT_FALSE(owners.has_value());
    EXPECT_EQ(owners.error().code, shared_kernel::ErrorCode::BackendUnavailable);
    EXPECT_NE(owners.error().detail.find(missing.string()), std::string::npos);
}

TEST(ProcExternalProcessProbe, RejectsOversizedComm_AndAcceptsWithoutNewline)
{
    const FakeProc proc;
    // Nome conhecido seguido de lixo além de 64 bytes: não pode ser aceito como "openrgb".
    proc.addProcess("50", "openrgb" + std::string(200, 'x') + "\n");
    // Nome conhecido inteiro dentro de 64 bytes mas sem '\n' final continua válido.
    proc.addProcess("51", "headsetcontrol");

    ProcExternalProcessProbe probe(proc.root());
    const auto owners = probe.runningOwners();

    ASSERT_TRUE(owners.has_value());
    EXPECT_EQ(*owners, (Owners{ExternalOwner::HeadsetControl}));
}

} // namespace
} // namespace ghub::device_access
