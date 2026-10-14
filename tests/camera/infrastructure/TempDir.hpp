#pragma once

#include <unistd.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace ghub::camera::testing {

namespace fs = std::filesystem;

// Diretório temporário removido no destrutor (RAII).
class TempDir {
public:
    TempDir()
    {
        std::string pattern = (fs::temp_directory_path() / "ghub-v4l2-XXXXXX").string();
        if (::mkdtemp(pattern.data()) == nullptr) {
            throw std::runtime_error("mkdtemp falhou");
        }
        path_ = pattern;
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;
    ~TempDir()
    {
        std::error_code ignored;
        fs::permissions(path_, fs::perms::owner_all, fs::perm_options::add, ignored);
        fs::remove_all(path_, ignored);
    }

    [[nodiscard]] const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

} // namespace ghub::camera::testing
