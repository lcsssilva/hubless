#include "modules/camera/infrastructure/BackgroundCameraReader.hpp"

#include <QtConcurrent>

namespace ghub::camera {

BackgroundCameraReader::BackgroundCameraReader(CameraBackend& inner, std::chrono::milliseconds refreshInterval,
                                               QObject* parent)
    : QObject(parent), inner_(inner),
      cache_(shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "primeira leitura em andamento")),
      timer_(this)
{
    connect(&watcher_, &QFutureWatcher<Reading>::finished, this, &BackgroundCameraReader::onReadFinished);
    connect(&timer_, &QTimer::timeout, this, &BackgroundCameraReader::refresh);
    timer_.setInterval(refreshInterval);
    timer_.start();
    refresh();
}

BackgroundCameraReader::~BackgroundCameraReader()
{
    timer_.stop();
    // A leitura não é cancelável: espera terminar, pois `inner_` deve continuar válido até lá.
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
}

shared_kernel::Result<std::optional<CameraDescription>> BackgroundCameraReader::readCamera()
{
    return cache_;
}

void BackgroundCameraReader::refresh()
{
    if (running_) {
        return;
    }
    running_ = true;
    CameraBackend& inner = inner_;
    watcher_.setFuture(QtConcurrent::run([&inner] { return inner.readCamera(); }));
}

void BackgroundCameraReader::onReadFinished()
{
    cache_ = watcher_.result();
    running_ = false;
    emit cameraRead();
}

} // namespace ghub::camera
