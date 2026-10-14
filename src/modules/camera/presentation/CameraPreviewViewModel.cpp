#include "modules/camera/presentation/CameraPreviewViewModel.hpp"

#include <QDebug>

#include <cstdint>
#include <utility>

namespace ghub::camera {

namespace {

// Duplicada de CameraViewModel.cpp (função pequena; extrair exigiria um cabeçalho compartilhado de apresentação).
// Taxa em fps com vírgula decimal, no máximo 2 casas e sem zeros à direita ("7,5", "29,97", "30").
QString rateText(const FrameRate& rate)
{
    const std::uint64_t seconds = rate.seconds();
    const std::uint64_t hundredths = ((std::uint64_t{rate.frames()} * 100) + (seconds / 2)) / seconds;
    const auto whole = static_cast<qulonglong>(hundredths / 100);
    const auto fraction = static_cast<int>(hundredths % 100);
    auto text = QString::number(whole);
    if (fraction != 0) {
        text += QLatin1Char(',') + QStringLiteral("%1").arg(fraction, 2, 10, QLatin1Char('0'));
        if (text.endsWith(QLatin1Char('0'))) {
            text.chop(1);
        }
    }
    return text;
}

QString liveText(const PreviewMode& mode)
{
    return CameraPreviewViewModel::tr("ao vivo · %1 %2×%3 a %4 fps")
        .arg(QString::fromUtf8(mode.format.fourcc().data(), static_cast<qsizetype>(mode.format.fourcc().size())))
        .arg(mode.width)
        .arg(mode.height)
        .arg(rateText(mode.frameRate));
}

QString failureText(PreviewFailure reason)
{
    switch (reason) {
    case PreviewFailure::CameraNotDetected:
        return CameraPreviewViewModel::tr("câmera não detectada");
    case PreviewFailure::CameraUnavailable:
        return CameraPreviewViewModel::tr("leitura da câmera indisponível");
    case PreviewFailure::NoCompatibleMode:
        return CameraPreviewViewModel::tr("nenhum modo de vídeo compatível com a pré-visualização");
    case PreviewFailure::CameraBusy:
        return CameraPreviewViewModel::tr("câmera em uso por outro programa");
    case PreviewFailure::CaptureFailed:
        return CameraPreviewViewModel::tr("falha ao iniciar a captura");
    case PreviewFailure::StreamFailed:
        return CameraPreviewViewModel::tr("a captura foi interrompida");
    case PreviewFailure::NoFrames:
        return CameraPreviewViewModel::tr("a câmera parou de enviar imagens");
    }
    return {}; // inalcançável: o switch cobre todos os enumeradores
}

} // namespace

CameraPreviewViewModel::CameraPreviewViewModel(const StartCameraPreview& startPreview, PreviewRunner& runner,
                                               QObject* parent)
    : QObject(parent), startPreview_(&startPreview), runner_(&runner), status_(tr("parada"))
{
}

CameraPreviewViewModel::~CameraPreviewViewModel()
{
    if (active()) {
        runner_->stop();
    }
}

void CameraPreviewViewModel::toggle()
{
    if (active()) {
        stop();
        return;
    }
    const auto result = startPreview_->execute();
    if (!result.has_value()) {
        fail(result.error().reason, QString::fromStdString(result.error().detail));
        return;
    }
    apply(State::Starting, *result, tr("iniciando…"));
}

void CameraPreviewViewModel::stop()
{
    if (!active()) {
        return;
    }
    runner_->stop();
    apply(State::Stopped, std::nullopt, tr("parada"));
}

void CameraPreviewViewModel::onStarted()
{
    if (state_ == State::Stopped || !mode_.has_value()) {
        return;
    }
    apply(State::Live, mode_, liveText(*mode_));
}

void CameraPreviewViewModel::onFrame(const QImage& image)
{
    if (state_ == State::Stopped) {
        return;
    }
    emit frameReady(image);
}

void CameraPreviewViewModel::onEnded(PreviewFailure reason, const QString& detail)
{
    if (state_ == State::Stopped) {
        return;
    }
    fail(reason, detail);
}

void CameraPreviewViewModel::fail(PreviewFailure reason, const QString& detail)
{
    // O detalhe pode conter caminho de dispositivo: vai ao log, nunca ao status exibido.
    if (!detail.isEmpty()) {
        qWarning().noquote() << "camera: pré-visualização falhou:" << detail;
    }
    apply(State::Stopped, std::nullopt, failureText(reason));
}

void CameraPreviewViewModel::apply(State state, const std::optional<PreviewMode>& mode, QString status)
{
    if (state == state_ && mode == mode_ && status == status_) {
        return;
    }
    state_ = state;
    mode_ = mode;
    status_ = std::move(status);
    emit previewChanged();
}

} // namespace ghub::camera
