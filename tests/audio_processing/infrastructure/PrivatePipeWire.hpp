#pragma once

// Daemon `pipewire` PRIVADO para os testes de integração (INC-010, regra de segurança): socket e configuração em um
// diretório temporário próprio, sem módulos de ALSA/pulse/jack/rt/dbus/portal. Nunca toca o PipeWire da sessão.
// Falha (exceção) se o socket privado não aparecer; mata o daemon e apaga o diretório no destrutor.

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <fcntl.h>
#include <poll.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

namespace ghub::audio_processing::testing {

class PrivatePipeWire {
public:
    PrivatePipeWire()
    {
        char pattern[] = "/tmp/ghub-pw-XXXXXX";
        if (mkdtemp(pattern) == nullptr) {
            throw std::runtime_error("mkdtemp falhou");
        }
        dir_ = pattern;
        socket_ = dir_ + "/sock";
        writeConfig();
        start();
    }

    ~PrivatePipeWire()
    {
        kill();
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec); // diretório criado por este objeto, caminho fixo /tmp/ghub-pw-*
    }

    PrivatePipeWire(const PrivatePipeWire&) = delete;
    PrivatePipeWire& operator=(const PrivatePipeWire&) = delete;
    PrivatePipeWire(PrivatePipeWire&&) = delete;
    PrivatePipeWire& operator=(PrivatePipeWire&&) = delete;

    [[nodiscard]] const std::string& socketPath() const noexcept { return socket_; }
    [[nodiscard]] const std::string& directory() const noexcept { return dir_; }

    // Mata o daemon (SIGKILL) e espera; idempotente.
    void kill()
    {
        if (pid_ <= 0) {
            return;
        }
        ::kill(pid_, SIGKILL);
        int status = 0;
        waitpid(pid_, &status, 0);
        pid_ = -1;
    }

    // Destrói um global do daemon PRIVADO (`pw-cli -r <socket privado> destroy <id>`); true se o comando saiu com 0.
    [[nodiscard]] bool destroyGlobal(std::uint32_t id) const
    {
        const std::string command = "PIPEWIRE_RUNTIME_DIR='" + dir_ + "' XDG_RUNTIME_DIR='" + dir_ +
                                    "' timeout 5 pw-cli -r '" + socket_ + "' destroy " + std::to_string(id) +
                                    " >/dev/null 2>&1";
        return std::system(command.c_str()) == 0;
    }

    // Saída de `pw-dump -r <socket privado>` (somente leitura do daemon privado), com limite de 5 s.
    [[nodiscard]] std::string dump() const
    {
        const std::string command = "PIPEWIRE_RUNTIME_DIR='" + dir_ + "' XDG_RUNTIME_DIR='" + dir_ +
                                    "' timeout 5 pw-dump -r '" + socket_ + "' 2>&1";
        std::string out;
        if (FILE* pipe = popen(command.c_str(), "r")) {
            char buffer[4096];
            while (const auto n = fread(buffer, 1, sizeof buffer, pipe)) {
                out.append(buffer, n);
            }
            pclose(pipe);
        }
        return out;
    }

private:
    void writeConfig() const
    {
        std::ofstream conf(dir_ + "/pw.conf");
        conf << "context.properties = {\n"
                "    core.name = ghub-test-"
             << getpid()
             << "\n"
                "    core.daemon = true\n"
                "    support.dbus = false\n"
                "    log.level = 1\n"
                "}\n"
                "context.spa-libs = {\n"
                "    audio.convert.* = audioconvert/libspa-audioconvert\n"
                "    support.*       = support/libspa-support\n"
                "}\n"
                "context.modules = [\n"
                "    { name = libpipewire-module-protocol-native args = { sockets = [ { name = \""
             << socket_
             << "\" } ] } }\n"
                "    { name = libpipewire-module-access }\n"
                "    { name = libpipewire-module-client-node }\n"
                "    { name = libpipewire-module-client-device }\n"
                "    { name = libpipewire-module-adapter }\n"
                "    { name = libpipewire-module-metadata }\n"
                "    { name = libpipewire-module-spa-node-factory }\n"
                "    { name = libpipewire-module-link-factory }\n"
                "]\n"
                "context.objects = [\n"
                "    { factory = metadata args = { metadata.name = default } }\n"
                "    { factory = spa-node-factory\n"
                "      args = { factory.name = support.node.driver node.name = fake.driver node.group = fake.driver "
                "priority.driver = 20000 } }\n"
                "    { factory = adapter\n"
                "      args = { factory.name = support.null-audio-sink node.name = fake.g733 media.class = Audio/Sink "
                "object.linger = true audio.position = [ FL FR ] } }\n"
                "]\n";
    }

    void start()
    {
        pid_ = fork();
        if (pid_ < 0) {
            throw std::runtime_error("fork falhou");
        }
        if (pid_ == 0) {
            prctl(PR_SET_PDEATHSIG, SIGKILL);
            clearenv();
            setenv("PATH", "/usr/bin:/bin", 1);
            setenv("HOME", dir_.c_str(), 1);
            setenv("XDG_RUNTIME_DIR", dir_.c_str(), 1);
            setenv("PIPEWIRE_RUNTIME_DIR", dir_.c_str(), 1);
            const std::string conf = dir_ + "/pw.conf";
            const int null = open("/dev/null", O_RDWR);
            dup2(null, 0);
            dup2(null, 1);
            dup2(null, 2);
            execl("/usr/bin/pipewire", "pipewire", "-c", conf.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        waitForSocket();
    }

    // Espera por condição (conexão aceita) com prazo de 10 s; falha se o daemon privado não subir.
    void waitForSocket()
    {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (std::chrono::steady_clock::now() < end) {
            int status = 0;
            if (waitpid(pid_, &status, WNOHANG) == pid_) {
                pid_ = -1;
                throw std::runtime_error("o daemon pipewire privado terminou antes de abrir o socket");
            }
            if (canConnect()) {
                return;
            }
            poll(nullptr, 0, 20); // intervalo entre tentativas de uma espera com prazo
        }
        kill();
        throw std::runtime_error("socket do pipewire privado não apareceu: " + socket_);
    }

    [[nodiscard]] bool canConnect() const
    {
        const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        socket_.copy(addr.sun_path, sizeof addr.sun_path - 1);
        const bool ok = connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof addr) == 0;
        close(fd);
        return ok;
    }

    std::string dir_;
    std::string socket_;
    pid_t pid_ = -1;
};

} // namespace ghub::audio_processing::testing
