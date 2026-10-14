// Teste físico opt-in (INC-011, fase A): diagnóstico SOMENTE LEITURA da memória onboard do G703 (046d:4086) pelo
// nó HID++ do receptor c539 (interface 2), com o canal HID++ compartilhado (ADR-0017). Envia apenas Root
// fn0/fn1, FeatureSet fn0/fn1 e 0x8100 fn0/fn2/fn4/fn5; a G703OnboardPolicy (profileCount 0) recusa qualquer outra
// função (fn1 modo, fn3 perfil atual, fn6 com setor; este teste não chama fn7/fn8). Nada é escrito no mouse. Executar
// com GHUB_HW_TESTS=1 e GHUB_HW_POINTER_ONBOARD=1, com ratbagd/solaar/piper/openrgb parados. Com GHUB_HW_DUMP_DIR=<dir>
// grava cada setor lido em <dir>/g703-sector-XXXX.bin (cópia byte a byte para restauração).
#include "modules/device_access/infrastructure/ProcExternalProcessProbe.hpp"
#include "modules/device_access/infrastructure/SteadyClock.hpp"
#include "modules/pointer/infrastructure/G703NodeLocator.hpp"
#include "modules/pointer/infrastructure/G703OnboardPolicy.hpp"
#include "platform/hidpp/HidppCapabilities.hpp"
#include "platform/hidpp/HidppChannel.hpp"
#include "platform/hidpp/HidrawDevice.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace ghub::pointer::infrastructure; // NOLINT(google-build-using-namespace)

namespace {

bool enabled(const char* name)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* value = std::getenv(name);
    return value != nullptr && std::string_view{value} == "1";
}

std::optional<std::filesystem::path> dumpDir()
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): idem.
    const char* value = std::getenv("GHUB_HW_DUMP_DIR");
    if (value == nullptr || std::string_view{value}.empty()) {
        return std::nullopt;
    }
    return std::filesystem::path{value};
}

std::string hex(const std::uint8_t* data, std::size_t size)
{
    std::ostringstream out;
    for (std::size_t i = 0; i < size; ++i) {
        out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << (i + 1 < size ? " " : "");
    }
    return out.str();
}

std::string hex16(unsigned value)
{
    std::ostringstream out;
    out << std::hex << std::setw(4) << std::setfill('0') << value;
    return out.str();
}

unsigned be16(const std::uint8_t* p)
{
    return (static_cast<unsigned>(p[0]) << 8U) | p[1];
}

bool anotherOwnerRunning(std::string& which)
{
    ghub::device_access::ProcExternalProcessProbe probe;
    const auto owners = probe.runningOwners();
    if (!owners.has_value()) {
        which = "sonda de processos indisponível: " + owners.error().detail;
        return true;
    }
    if (!owners->empty()) {
        which = std::string{ghub::device_access::processName(owners->front())};
        return true;
    }
    return false;
}

void printError(const ghub::platform::hidpp::HidppError& error)
{
    std::cout << "erro tipo " << static_cast<int>(error.kind) << " código " << static_cast<int>(error.code) << " ("
              << error.detail << ")\n";
}

// CRC-CCITT do libratbag (src/hidpp-generic.c:380-405@8235b5bb), semente 0xFFFF, sobre sector_size - 2 bytes.
std::uint16_t crcCcitt(const std::uint8_t* data, std::size_t length)
{
    auto crc = static_cast<std::uint16_t>(0xFFFF);
    for (std::size_t i = 0; i < length; ++i) {
        const auto temp = static_cast<std::uint16_t>((crc >> 8) ^ data[i]);
        crc = static_cast<std::uint16_t>(crc << 8);
        auto quick = static_cast<std::uint16_t>(temp ^ (temp >> 4));
        crc = static_cast<std::uint16_t>(crc ^ quick);
        quick = static_cast<std::uint16_t>(quick << 5);
        crc = static_cast<std::uint16_t>(crc ^ quick);
        quick = static_cast<std::uint16_t>(quick << 7);
        crc = static_cast<std::uint16_t>(crc ^ quick);
    }
    return crc;
}

// Setor inteiro em passos de 16 bytes; o último trecho curto relê de sector_size-16 (libratbag
// hidpp20.c:1900-1915). Erro do dispositivo: imprime e devolve nullopt, sem retentativa.
std::optional<std::vector<std::uint8_t>> readSector(ghub::platform::hidpp::HidppChannel& channel,
                                                    std::uint8_t deviceIndex, unsigned sector, unsigned sectorSize)
{
    std::vector<std::uint8_t> data(sectorSize, 0);
    for (unsigned offset = 0; offset < sectorSize; offset += 16) {
        offset = (sectorSize - offset < 16) ? sectorSize - 16 : offset;
        const std::vector<std::uint8_t> params{
            static_cast<std::uint8_t>(sector >> 8U), static_cast<std::uint8_t>(sector & 0xFFU),
            static_cast<std::uint8_t>(offset >> 8U), static_cast<std::uint8_t>(offset & 0xFFU)};
        const auto reply = channel.call(deviceIndex, 0x8100, 5, params);
        if (!reply.has_value()) {
            std::cout << "  setor 0x" << hex16(sector) << " offset 0x" << hex16(offset) << ": ";
            printError(reply.error());
            return std::nullopt;
        }
        std::copy(reply->begin(), reply->end(), data.begin() + offset);
    }
    return data;
}

void dumpSector(const char* kind, unsigned sector, const std::vector<std::uint8_t>& data)
{
    std::cout << "  setor 0x" << hex16(sector) << " (" << kind << ")\n";
    for (std::size_t offset = 0; offset < data.size(); offset += 16) {
        std::cout << "    " << hex16(static_cast<unsigned>(offset)) << ": "
                  << hex(data.data() + offset, std::min<std::size_t>(16, data.size() - offset)) << "\n";
    }
    const unsigned stored = be16(&data[data.size() - 2]);
    const unsigned computed = crcCcitt(data.data(), data.size() - 2);
    std::cout << "    CRC calculado 0x" << hex16(computed) << " gravado 0x" << hex16(stored) << ": "
              << (stored == computed ? "CRC ok" : "CRC inválido") << "\n";
    if (const auto dir = dumpDir()) {
        std::ofstream out{*dir / ("g703-sector-" + hex16(sector) + ".bin"), std::ios::binary};
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        std::cout << "    gravado em " << (out ? "arquivo" : "FALHA ao gravar arquivo") << "\n";
    }
}

} // namespace

TEST(PointerHidppHardware, ReadsOnboardProfilesOfG703)
{
    if (!enabled("GHUB_HW_TESTS") || !enabled("GHUB_HW_POINTER_ONBOARD")) {
        GTEST_SKIP() << "teste físico: defina GHUB_HW_TESTS=1 e GHUB_HW_POINTER_ONBOARD=1 (leitura onboard do G703)";
    }
    std::string owner;
    if (anotherOwnerRunning(owner)) {
        GTEST_SKIP() << "outro software disputa o hidraw: " << owner;
    }
    const auto node = G703NodeLocator{}.locate();
    ASSERT_TRUE(node.has_value()) << node.error().detail;
    std::cout << "G703: " << node->devicePath.string() << " dev=" << static_cast<int>(node->deviceIndex) << "\n";
    auto device = ghub::platform::hidpp::HidrawDevice::open(node->devicePath);
    ASSERT_TRUE(device.has_value()) << device.error().detail;
    ghub::device_access::SteadyClock clock;
    // Só leitura: o canal recebe a política do G703 com profileCount 0, que recusa qualquer fn6 (e fn3 sempre).
    const G703OnboardPolicy policy;
    ghub::platform::hidpp::HidppChannel channel{std::move(*device), clock, policy};
    const std::uint8_t index = node->deviceIndex;

    ghub::platform::hidpp::HidppCapabilities capabilities{channel};
    const auto features = capabilities.enumerateFeatures(index);
    ASSERT_TRUE(features.has_value()) << "enumeração: erro " << static_cast<int>(features.error().kind) << " "
                                      << features.error().detail;
    for (const auto& feature : *features) {
        std::cout << "  índice " << static_cast<int>(feature.index) << " -> 0x" << hex16(feature.featureId)
                  << " tipo 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(feature.type)
                  << std::dec << "\n";
    }

    const auto descr = channel.call(index, 0x8100, 0);
    ASSERT_TRUE(descr.has_value()) << "fn0 GetProfilesDescr: erro " << static_cast<int>(descr.error().kind)
                                   << " código " << static_cast<int>(descr.error().code) << " " << descr.error().detail;
    const auto& d = *descr;
    const unsigned profileCount = d[3];
    const unsigned profileCountOob = d[4];
    const unsigned sectorSize = be16(&d[7]);
    std::cout << "fn0 GetProfilesDescr: " << hex(d.data(), d.size()) << "\n"
              << "  memory_model=" << static_cast<int>(d[0]) << " profile_format=" << static_cast<int>(d[1])
              << " macro_format=" << static_cast<int>(d[2]) << " profile_count=" << profileCount
              << " profile_count_oob=" << profileCountOob << " button_count=" << static_cast<int>(d[5])
              << " sector_count=" << static_cast<int>(d[6]) << " sector_size=" << sectorSize
              << " mechanical_layout=" << static_cast<int>(d[9]) << " various_info=" << static_cast<int>(d[10]) << "\n";
    if (const auto mode = channel.call(index, 0x8100, 2); mode.has_value()) {
        std::cout << "fn2 GetOnboardMode: " << static_cast<int>((*mode)[0]) << " (1 = onboard, 2 = host)\n";
    } else {
        std::cout << "fn2 GetOnboardMode: ";
        printError(mode.error());
    }
    if (const auto current = channel.call(index, 0x8100, 4); current.has_value()) {
        std::cout << "fn4 GetCurrentProfile: [0]=" << static_cast<int>((*current)[0])
                  << " [1]=" << static_cast<int>((*current)[1]) << "\n";
    } else {
        std::cout << "fn4 GetCurrentProfile: ";
        printError(current.error());
    }
    if (sectorSize < 16 || sectorSize > 4096) {
        FAIL() << "sector_size fora do limite de segurança (16..4096): " << sectorSize << "; nenhum setor lido";
    }

    // Diretório 0x0000: entradas de 4 bytes (endereço BE16, habilitado = byte [2]) até 0xFFFF.
    if (const auto directory = readSector(channel, index, 0x0000, sectorSize)) {
        dumpSector("diretório", 0x0000, *directory);
        for (std::size_t at = 0; at + 4 <= directory->size(); at += 4) {
            const unsigned address = be16(&(*directory)[at]);
            if (address == 0xFFFF) {
                break;
            }
            std::cout << "    entrada: setor 0x" << hex16(address)
                      << " habilitado=" << static_cast<int>((*directory)[at + 2]) << "\n";
        }
    }
    for (unsigned i = 0; i < std::min(profileCount, 8U); ++i) {
        if (const auto data = readSector(channel, index, 0x0001 + i, sectorSize)) {
            dumpSector("perfil de usuário", 0x0001 + i, *data);
        }
    }
    for (unsigned i = 0; i < std::min(profileCountOob, 8U); ++i) {
        if (const auto data = readSector(channel, index, 0x0101 + i, sectorSize)) {
            dumpSector("perfil de ROM", 0x0101 + i, *data);
        }
    }
}
