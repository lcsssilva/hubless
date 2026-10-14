// Teste físico opt-in (INC-008 parte A, ADR-0013): LEITURA HID++ 2.0 do G915 (nó do receptor c547) e do G733.
// Só envia Root fn0/fn1, FeatureSet fn0/fn1 e GetInfo (fn0) das features de iluminação, e da 0x8100 (Onboard Profiles)
// apenas fn0/fn2/fn4/fn5 (somente leitura). Nenhuma escrita de estado.
// Executar com GHUB_HW_TESTS=1 e GHUB_HW_LIGHTING_READ=1; registrar a saída em evidence/ e no VERIFICATION_LOG.
// O diagnóstico dos perfis onboard do G915 exige GHUB_HW_TESTS=1 e GHUB_HW_LIGHTING_ONBOARD=1.
#include "modules/device_access/infrastructure/ProcExternalProcessProbe.hpp"
#include "modules/device_access/infrastructure/SteadyClock.hpp"
#include "modules/lighting/infrastructure/HidppCapabilities.hpp"
#include "modules/lighting/infrastructure/HidppChannel.hpp"
#include "modules/lighting/infrastructure/HidppNodeLocator.hpp"
#include "modules/lighting/infrastructure/HidrawDevice.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace ghub::lighting::infrastructure; // NOLINT(google-build-using-namespace)

namespace {

bool enabled(const char* name)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* value = std::getenv(name);
    return value != nullptr && std::string_view{value} == "1";
}

std::string hex(const std::uint8_t* data, std::size_t size)
{
    std::ostringstream out;
    for (std::size_t i = 0; i < size; ++i) {
        out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << (i + 1 < size ? " " : "");
    }
    return out.str();
}

// Falha o teste (SKIP) se algum software conhecido por disputar o hidraw estiver rodando.
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

void readNode(const char* label, const HidppNode& node)
{
    std::cout << "== " << label << " dev=" << static_cast<int>(node.deviceIndex) << "\n";
    auto device = HidrawDevice::open(node.devicePath);
    ASSERT_TRUE(device.has_value()) << device.error().detail;
    ghub::device_access::SteadyClock clock;
    HidppChannel channel{std::move(*device), clock};
    HidppCapabilities capabilities{channel};

    const auto features = capabilities.enumerateFeatures(node.deviceIndex);
    ASSERT_TRUE(features.has_value()) << "erro " << static_cast<int>(features.error().kind) << " código "
                                      << static_cast<int>(features.error().code) << " " << features.error().detail;
    for (const auto& feature : *features) {
        std::cout << "  índice " << static_cast<int>(feature.index) << " -> 0x" << std::hex << std::setw(4)
                  << std::setfill('0') << feature.featureId << " tipo 0x" << std::setw(2)
                  << static_cast<int>(feature.type) << std::dec << "\n";
    }
    // GetInfo como o Solaar 1.1.20 (hidpp20.py:1330-1401): 0x8071 fn0 com FF FF 00 (dispositivo), [zona FF 00]
    // e [zona efeito 00]; 0x8070 fn0 sem parâmetros, fn1 [zona FF 00], fn2 [zona efeito 00]. Só leituras.
    // Erros do dispositivo são impressos, não interrompem: o objetivo é registrar o que cada um responde.
    const auto query = [&](std::uint16_t id, std::uint8_t function,
                           std::initializer_list<std::uint8_t> params) -> std::optional<HidppChannel::Response> {
        const std::vector<std::uint8_t> bytes{params};
        const auto reply = channel.call(node.deviceIndex, id, function, bytes);
        std::cout << "  0x" << std::hex << id << " fn" << std::dec << static_cast<int>(function) << " ["
                  << hex(bytes.data(), bytes.size()) << "]: ";
        if (!reply.has_value()) {
            std::cout << "erro tipo " << static_cast<int>(reply.error().kind) << " código "
                      << static_cast<int>(reply.error().code) << " (" << reply.error().detail << ")\n";
            return std::nullopt;
        }
        std::cout << hex(reply->data(), reply->size()) << "\n";
        return *reply;
    };
    constexpr int maxEntries = 16;
    if (const auto deviceInfo = query(HidppCapabilities::featureEffects, 0, {0xFF, 0xFF, 0x00})) {
        const int zones = std::min<int>((*deviceInfo)[2], maxEntries);
        for (int zone = 0; zone < zones; ++zone) {
            const auto z = static_cast<std::uint8_t>(zone);
            if (const auto info = query(HidppCapabilities::featureEffects, 0, {z, 0xFF, 0x00})) {
                const int effects = std::min<int>((*info)[4], maxEntries);
                for (int effect = 0; effect < effects; ++effect) {
                    query(HidppCapabilities::featureEffects, 0, {z, static_cast<std::uint8_t>(effect), 0x00});
                }
            }
        }
    }
    if (const auto deviceInfo = query(HidppCapabilities::featureColorLed, 0, {})) {
        const int zones = std::min<int>((*deviceInfo)[0], maxEntries);
        for (int zone = 0; zone < zones; ++zone) {
            const auto z = static_cast<std::uint8_t>(zone);
            if (const auto info = query(HidppCapabilities::featureColorLed, 1, {z, 0xFF, 0x00})) {
                const int effects = std::min<int>((*info)[3], maxEntries);
                for (int effect = 0; effect < effects; ++effect) {
                    query(HidppCapabilities::featureColorLed, 2, {z, static_cast<std::uint8_t>(effect), 0x00});
                }
            }
        }
    }
    query(HidppCapabilities::featurePerKey, 0, {});
}

// CRC-CCITT do libratbag: src/hidpp-generic.c:380-405@8235b5bb (hidpp_crc_ccitt): semente 0xFFFF; por byte:
// temp = (crc >> 8) ^ byte; crc <<= 8; quick = temp ^ (temp >> 4); crc ^= quick; quick <<= 5; crc ^= quick;
// quick <<= 7; crc ^= quick (equivale ao polinômio 0x1021). Uso: hidpp20.c:1927 (sobre sector_size - 2 bytes).
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

void printError(const HidppError& error)
{
    std::cout << "erro tipo " << static_cast<int>(error.kind) << " código " << static_cast<int>(error.code) << " ("
              << error.detail << ")\n";
}

// Lê um setor inteiro (libratbag hidpp20.c:1900-1915): passos de 16 bytes; o último trecho curto relê de
// sector_size-16. Em erro do dispositivo imprime e devolve nullopt (sem retentativa).
std::optional<std::vector<std::uint8_t>> readSector(HidppChannel& channel, std::uint8_t deviceIndex, unsigned sector,
                                                    unsigned sectorSize)
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
}

} // namespace

TEST(LightingHidppHardware, ReadsFeaturesAndInfoOfG915AndG733)
{
    if (!enabled("GHUB_HW_TESTS") || !enabled("GHUB_HW_LIGHTING_READ")) {
        GTEST_SKIP() << "teste físico: defina GHUB_HW_TESTS=1 e GHUB_HW_LIGHTING_READ=1 (leitura HID++ de G915/G733)";
    }
    std::string owner;
    if (anotherOwnerRunning(owner)) {
        GTEST_SKIP() << "outro software disputa o hidraw: " << owner;
    }
    const HidppNodeLocator locator;
    const auto g915 = locator.locateG915();
    const auto g733 = locator.locateG733();
    std::cout << "G915: " << (g915 ? g915->devicePath.string() : g915.error().detail) << "\n";
    std::cout << "G733: " << (g733 ? g733->devicePath.string() : g733.error().detail) << "\n";
    ASSERT_TRUE(g915.has_value() || g733.has_value()) << "nenhum dos dois dispositivos encontrado";
    if (g915.has_value()) {
        readNode("G915 (nó do receptor c547)", *g915);
    }
    if (g733.has_value()) {
        readNode("G733", *g733);
    }
}

// Diagnóstico SOMENTE LEITURA dos perfis onboard do G915 (0x8100 fn0/fn2/fn4/fn5). Layout do GetProfilesDescr:
// libratbag src/hidpp20.h:894-906. Nada é escrito no teclado.
TEST(LightingHidppHardware, ReadsOnboardProfilesOfG915)
{
    if (!enabled("GHUB_HW_TESTS") || !enabled("GHUB_HW_LIGHTING_ONBOARD")) {
        GTEST_SKIP()
            << "teste físico: defina GHUB_HW_TESTS=1 e GHUB_HW_LIGHTING_ONBOARD=1 (leitura dos perfis onboard)";
    }
    std::string owner;
    if (anotherOwnerRunning(owner)) {
        GTEST_SKIP() << "outro software disputa o hidraw: " << owner;
    }
    const HidppNodeLocator locator;
    const auto g915 = locator.locateG915();
    ASSERT_TRUE(g915.has_value()) << g915.error().detail;
    auto device = HidrawDevice::open(g915->devicePath);
    ASSERT_TRUE(device.has_value()) << device.error().detail;
    ghub::device_access::SteadyClock clock;
    HidppChannel channel{std::move(*device), clock};
    const std::uint8_t index = g915->deviceIndex;

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
                  << " [1]=" << static_cast<int>((*current)[1]) << " (perfil ativo = [1])\n";
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
        const unsigned sector = 0x0001 + i;
        if (const auto data = readSector(channel, index, sector, sectorSize)) {
            dumpSector("perfil de usuário", sector, *data);
        }
    }
    for (unsigned i = 0; i < std::min(profileCountOob, 8U); ++i) {
        const unsigned sector = 0x0101 + i;
        if (const auto data = readSector(channel, index, sector, sectorSize)) {
            dumpSector("perfil de ROM", sector, *data);
        }
    }
}
