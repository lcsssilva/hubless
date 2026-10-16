#pragma once

#include "modules/battery/infrastructure/PassiveReportSource.hpp"
#include "shared_kernel/Result.hpp"

#include <QObject>
#include <QSocketNotifier>
#include <QTimer>

#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>

namespace ghub::battery {

// Escuta passiva do G733 (ADR-0026, adendo): abre o hidraw só para leitura e emite headsetEvent() quando o headset
// envia por conta própria o evento da feature 0x1f20 (ligou/desligou). Não escreve nada, não usa thread e nunca
// registra o conteúdo dos relatórios (podem ser teclas do headset). Falha ao localizar/abrir ou erro de leitura
// (ex.: receptor removido) fecha a fonte e tenta de novo após `retryInterval`.
class HidppHeadsetEventWatcher final : public QObject {
    Q_OBJECT

public:
    using Locator = std::function<shared_kernel::Result<std::filesystem::path>()>;
    using Opener =
        std::function<std::expected<std::unique_ptr<PassiveReportSource>, int>(const std::filesystem::path&)>;

    HidppHeadsetEventWatcher(Locator locator, Opener opener, std::chrono::milliseconds retryInterval,
                             QObject* parent = nullptr);
    ~HidppHeadsetEventWatcher() override;

    void start();

signals:
    void headsetEvent();

private:
    void tryOpen();
    void onReadable();
    void closeAndRetry();

    Locator locator_;
    Opener opener_;
    QTimer retryTimer_;
    std::unique_ptr<PassiveReportSource> source_;
    std::unique_ptr<QSocketNotifier> notifier_;
};

} // namespace ghub::battery
