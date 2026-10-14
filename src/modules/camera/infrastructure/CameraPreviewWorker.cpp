#include "modules/camera/infrastructure/CameraPreviewWorker.hpp"

#include "modules/camera/infrastructure/FrameDecoder.hpp"
#include "shared_kernel/DomainError.hpp"

#include <QMetaObject>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace ghub::camera {

CameraPreviewWorker::CameraPreviewWorker(VideoCapture& capture, std::chrono::milliseconds frameTimeout,
                                         std::chrono::milliseconds stallLimit, QObject* parent)
    : QObject(parent), capture_(capture), frameTimeout_(frameTimeout), stallLimit_(stallLimit)
{
}

CameraPreviewWorker::~CameraPreviewWorker()
{
    stop();
}

void CameraPreviewWorker::start(const PreviewMode& mode)
{
    stop();
    dropped_ = 0;
    const std::uint64_t generation = generation_;
    thread_ = std::jthread([this, mode, generation](const std::stop_token& token) { run(token, mode, generation); });
}

void CameraPreviewWorker::stop()
{
    if (!thread_.joinable()) {
        return;
    }
    thread_.request_stop();
    thread_.join(); // a sessão é destruída dentro de run(); depois do join a câmera está liberada
    ++generation_;  // invalida o que a thread já enfileirou
    const std::scoped_lock lock(frameMutex_);
    pendingFrame_ = QImage();
    frameNotified_ = false;
}

void CameraPreviewWorker::run(const std::stop_token& stopToken, const PreviewMode& mode, std::uint64_t generation)
{
    auto started = capture_.start(mode);
    if (!started.has_value()) {
        const auto reason = started.error().code == shared_kernel::ErrorCode::DeviceBusy
                                ? PreviewFailure::CameraBusy
                                : PreviewFailure::CaptureFailed;
        postEnded(generation, reason, QString::fromStdString(started.error().detail));
        return;
    }
    std::unique_ptr<VideoCaptureSession> session = std::move(*started);
    if (session == nullptr) {
        postEnded(generation, PreviewFailure::CaptureFailed, QStringLiteral("a captura não devolveu sessão"));
        return;
    }
    postStarted(generation);

    std::vector<std::byte> buffer;
    auto lastValid = std::chrono::steady_clock::now();
    while (!stopToken.stop_requested()) {
        auto read = session->readFrame(frameTimeout_, buffer);
        if (!read.has_value()) {
            QString detail = QString::fromStdString(read.error().detail);
            session.reset(); // libera a câmera antes de avisar
            postEnded(generation, PreviewFailure::StreamFailed, std::move(detail));
            return;
        }
        if (*read) {
            QImage image = decodeFrame(mode, buffer);
            if (image.isNull()) {
                ++dropped_;
            } else {
                lastValid = std::chrono::steady_clock::now();
                postFrame(generation, std::move(image));
            }
        }
        if (std::chrono::steady_clock::now() - lastValid >= stallLimit_) {
            session.reset();
            postEnded(generation, PreviewFailure::NoFrames, {});
            return;
        }
    }
}

void CameraPreviewWorker::postStarted(std::uint64_t generation)
{
    QMetaObject::invokeMethod(
        this,
        [this, generation] {
            if (generation == generation_) {
                emit started();
            }
        },
        Qt::QueuedConnection);
}

void CameraPreviewWorker::postFrame(std::uint64_t generation, QImage image)
{
    bool notify = false;
    {
        const std::scoped_lock lock(frameMutex_);
        pendingFrame_ = std::move(image); // substitui um quadro ainda não entregue
        notify = !frameNotified_;
        frameNotified_ = true;
    }
    if (notify) {
        QMetaObject::invokeMethod(this, [this, generation] { deliverFrame(generation); }, Qt::QueuedConnection);
    }
}

void CameraPreviewWorker::deliverFrame(std::uint64_t generation)
{
    if (generation != generation_) {
        return; // execução já encerrada por stop(); o estado compartilhado foi zerado lá
    }
    QImage image;
    {
        const std::scoped_lock lock(frameMutex_);
        image = std::move(pendingFrame_);
        pendingFrame_ = QImage();
        frameNotified_ = false;
    }
    if (!image.isNull()) {
        emit frameReady(image);
    }
}

void CameraPreviewWorker::postEnded(std::uint64_t generation, PreviewFailure reason, QString detail)
{
    QMetaObject::invokeMethod(
        this,
        [this, generation, reason, detail = std::move(detail)] {
            if (generation == generation_) {
                emit ended(reason, detail);
            }
        },
        Qt::QueuedConnection);
}

} // namespace ghub::camera
