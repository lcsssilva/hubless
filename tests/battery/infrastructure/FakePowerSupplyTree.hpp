#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

namespace ghub::battery::test {

// Árvore /sys/class/power_supply falsa em diretório temporário: <tmp>/class/<nome>/<atributo>,
// com `device` -> ../../devices/<diretório HID>, como no sysfs real.
class FakePowerSupplyTree {
public:
    FakePowerSupplyTree()
    {
        std::string pattern = (std::filesystem::temp_directory_path() / "ghub-sysfs-XXXXXX").string();
        if (::mkdtemp(pattern.data()) == nullptr) {
            throw std::runtime_error("mkdtemp falhou");
        }
        base_ = pattern;
        std::filesystem::create_directories(classRoot());
    }
    FakePowerSupplyTree(const FakePowerSupplyTree&) = delete;
    FakePowerSupplyTree& operator=(const FakePowerSupplyTree&) = delete;
    FakePowerSupplyTree(FakePowerSupplyTree&&) = delete;
    FakePowerSupplyTree& operator=(FakePowerSupplyTree&&) = delete;
    ~FakePowerSupplyTree()
    {
        std::error_code ignored;
        std::filesystem::remove_all(base_, ignored);
    }

    [[nodiscard]] std::filesystem::path classRoot() const { return base_ / "class"; }

    // Entrada de bateria de periférico com os atributos informados (o kernel termina cada valor com '\n').
    void addEntry(const std::string& name, const std::string& hidDirectory,
                  const std::map<std::string, std::string>& attributes)
    {
        const auto entry = classRoot() / name;
        std::filesystem::create_directories(entry);
        std::filesystem::create_directories(base_ / "devices" / hidDirectory);
        std::filesystem::create_directory_symlink(std::filesystem::path("../../devices") / hidDirectory,
                                                  entry / "device");
        for (const auto& [attribute, value] : attributes) {
            writeAttribute(name, attribute, value + "\n");
        }
    }

    void writeAttribute(const std::string& name, const std::string& attribute, const std::string& content) const
    {
        std::ofstream(classRoot() / name / attribute, std::ios::binary | std::ios::trunc) << content;
    }

private:
    std::filesystem::path base_;
};

// Atributos de bateria hidpp como observados no G915 em 2026-10-13 (evidence/2026-10-13-bateria-kernel.md).
inline std::map<std::string, std::string> hidppAttributes(const std::string& status, const std::string& capacity)
{
    return {{"type", "Battery"}, {"scope", "Device"}, {"status", status}, {"capacity", capacity}};
}

} // namespace ghub::battery::test
