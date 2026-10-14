// Teste físico opt-in (INC-011, ADR-0017): ESCRITA no setor do perfil onboard ativo do G703, em três etapas
// separadas, cada uma só com a sua variável (autorização do usuário na sessão; protocol-safety-auditor no
// VERIFICATION_LOG, INC-011). Sempre exige GHUB_HW_TESTS=1 e GHUB_HW_POINTER_WRITE=<etapa>:
//   A  regrava o setor ativo com os MESMOS bytes (só se ele for idêntico à cópia da evidência) e confere a releitura;
//   B  grava pelo caso de uso G703OnboardSettings os DPIs de GHUB_HW_G703_DPI (padrão "1600"), 1000 Hz, padrão 0;
//   C  restaura a cópia pelo caso de uso e confere que o setor voltou a ser idêntico à evidência.
// B e C exigem GHUB_HW_BACKUP_DIR (diretório da cópia de segurança do teste). Rodar com ratbagd/piper/solaar parados.
#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/infrastructure/ProcExternalProcessProbe.hpp"
#include "modules/device_access/infrastructure/SteadyClock.hpp"
#include "modules/pointer/application/G703OnboardSettings.hpp"
#include "modules/pointer/infrastructure/FileOnboardBackupStore.hpp"
#include "modules/pointer/infrastructure/G703NodeLocator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef GHUB_SOURCE_DIR
#error "GHUB_SOURCE_DIR precisa ser definido pelo CMake"
#endif

namespace {

using ghub::pointer::G703ProfileSettings;

std::string env(const char* name)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
}

bool stepEnabled(std::string_view step)
{
    return env("GHUB_HW_TESTS") == "1" && env("GHUB_HW_POINTER_WRITE") == step;
}

// Setor 0x0001 lido em 2026-10-14 12:55 (SHA-256 d918d03f…); a referência para A e C.
std::vector<std::uint8_t> evidenceSector()
{
    const std::filesystem::path path = std::filesystem::path{GHUB_SOURCE_DIR} /
                                       "docs/device-support/evidence/2026-10-14-g703-onboard-dump/g703-sector-0001.bin";
    std::ifstream in{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

std::string describe(const G703ProfileSettings& settings)
{
    std::ostringstream out;
    out << settings.reportRateHz() << " Hz, DPI";
    for (const auto dpi : settings.dpiStages()) {
        out << " " << dpi;
    }
    out << ", padrão " << settings.defaultStage();
    return out.str();
}

std::vector<std::uint16_t> requestedDpis()
{
    std::vector<std::uint16_t> dpis;
    std::istringstream in{env("GHUB_HW_G703_DPI").empty() ? std::string{"1600"} : env("GHUB_HW_G703_DPI")};
    for (std::string item; std::getline(in, item, ',');) {
        dpis.push_back(static_cast<std::uint16_t>(std::stoul(item)));
    }
    return dpis;
}

struct Rig {
    ghub::device_access::SteadyClock clock;
    ghub::device_access::ProcExternalProcessProbe probe;
    ghub::device_access::AccessRegistry registry{clock};
    ghub::pointer::infrastructure::G703OnboardConnector connector{ghub::pointer::infrastructure::G703NodeLocator{},
                                                                  clock};
    ghub::pointer::infrastructure::FileOnboardBackupStore backup;
    ghub::pointer::G703OnboardSettings useCase{probe, registry, connector, backup};

    explicit Rig(std::filesystem::path backupDir) : backup(std::move(backupDir)) {}
};

} // namespace

TEST(PointerOnboardWriteHardware, A_RewritesTheActiveSectorWithTheSameBytes)
{
    if (!stepEnabled("A")) {
        GTEST_SKIP() << "teste físico: GHUB_HW_TESTS=1 GHUB_HW_POINTER_WRITE=A (regrava os mesmos bytes)";
    }
    ghub::device_access::ProcExternalProcessProbe probe;
    const auto owners = probe.runningOwners();
    ASSERT_TRUE(owners.has_value() && owners->empty()) << "outro software disputa o mouse";
    const auto expected = evidenceSector();
    ASSERT_EQ(expected.size(), 255U) << "cópia da evidência ausente";

    ghub::device_access::SteadyClock clock;
    ghub::pointer::infrastructure::G703OnboardConnector connector{ghub::pointer::infrastructure::G703NodeLocator{},
                                                                  clock};
    auto memory = connector.open();
    ASSERT_TRUE(memory.has_value()) << memory.error().detail;
    const auto layout = (*memory)->describe();
    ASSERT_TRUE(layout.has_value()) << layout.error().detail;
    ASSERT_EQ(layout->sectorSize, 255);
    const auto mode = (*memory)->onboardMode();
    ASSERT_TRUE(mode.has_value() && *mode == 1) << "mouse fora do modo onboard";
    const auto current = (*memory)->currentProfileSector();
    ASSERT_TRUE(current.has_value() && *current == 1) << "perfil ativo não é o setor 0x0001";
    const auto before = (*memory)->readSector(1, layout->sectorSize);
    ASSERT_TRUE(before.has_value()) << before.error().detail;
    // Só regrava se o mouse ainda estiver exatamente como na evidência (cópia de restauração conhecida).
    ASSERT_EQ(*before, expected) << "o setor 0x0001 mudou desde a leitura de 12:55; nada foi escrito";

    std::cout << "escrevendo o setor 0x0001 com os mesmos 255 bytes\n";
    const auto written = (*memory)->writeSector(1, *before);
    ASSERT_TRUE(written.has_value()) << "escrita: " << written.error().detail;
    const auto after = (*memory)->readSector(1, layout->sectorSize);
    ASSERT_TRUE(after.has_value()) << "releitura: " << after.error().detail;
    EXPECT_EQ(*after, expected) << "releitura diferente do enviado";
    std::cout << "releitura idêntica à evidência\n";
}

TEST(PointerOnboardWriteHardware, B_WritesDpiThroughTheUseCase)
{
    if (!stepEnabled("B")) {
        GTEST_SKIP() << "teste físico: GHUB_HW_TESTS=1 GHUB_HW_POINTER_WRITE=B GHUB_HW_BACKUP_DIR=<dir>";
    }
    const auto backupDir = env("GHUB_HW_BACKUP_DIR");
    ASSERT_FALSE(backupDir.empty()) << "defina GHUB_HW_BACKUP_DIR";
    const auto settings = G703ProfileSettings::make(1000, requestedDpis(), 0);
    ASSERT_TRUE(settings.has_value()) << settings.error().detail;
    Rig rig{backupDir};
    const auto before = rig.useCase.read();
    ASSERT_TRUE(before.has_value()) << before.error().detail;
    std::cout << "antes: setor " << before->sector << ", " << describe(before->settings) << "\n";
    const auto result = rig.useCase.write(*settings);
    ASSERT_TRUE(result.has_value()) << result.error().detail;
    std::cout << "gravado e relido: " << describe(*result) << "\n";
}

TEST(PointerOnboardWriteHardware, C_RestoresTheBackupThroughTheUseCase)
{
    if (!stepEnabled("C")) {
        GTEST_SKIP() << "teste físico: GHUB_HW_TESTS=1 GHUB_HW_POINTER_WRITE=C GHUB_HW_BACKUP_DIR=<dir>";
    }
    const auto backupDir = env("GHUB_HW_BACKUP_DIR");
    ASSERT_FALSE(backupDir.empty()) << "defina GHUB_HW_BACKUP_DIR";
    Rig rig{backupDir};
    const auto result = rig.useCase.restoreBackup();
    ASSERT_TRUE(result.has_value()) << result.error().detail;
    std::cout << "restaurado: " << describe(*result) << "\n";

    ghub::device_access::SteadyClock clock;
    ghub::pointer::infrastructure::G703OnboardConnector connector{ghub::pointer::infrastructure::G703NodeLocator{},
                                                                  clock};
    auto memory = connector.open();
    ASSERT_TRUE(memory.has_value()) << memory.error().detail;
    ASSERT_TRUE((*memory)->describe().has_value());
    const auto sector = (*memory)->readSector(1, 255);
    ASSERT_TRUE(sector.has_value()) << sector.error().detail;
    EXPECT_EQ(*sector, evidenceSector()) << "o setor restaurado difere da evidência";
}
