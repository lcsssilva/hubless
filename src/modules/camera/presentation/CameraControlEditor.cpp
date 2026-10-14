#include "modules/camera/presentation/CameraControlEditor.hpp"

#include <QDebug>
#include <QtConcurrent>

#include <utility>

namespace ghub::camera {

namespace {

QString successText(const CameraControl& control)
{
    QString value;
    const auto current = control.currentValue();
    if (!current.has_value()) {
        value = CameraControlEditor::tr("valor indisponível");
    } else if (control.kind() == ControlKind::Boolean) {
        value = *current != 0 ? CameraControlEditor::tr("ligado") : CameraControlEditor::tr("desligado");
    } else if (const auto option = control.currentOption(); control.kind() == ControlKind::Menu && option.has_value()) {
        value = QString::fromStdString(option->label);
    } else {
        value = QString::number(*current);
    }
    return QStringLiteral("%1: %2").arg(QString::fromStdString(control.name()), value);
}

QString failureText(shared_kernel::ErrorCode code)
{
    switch (code) {
    case shared_kernel::ErrorCode::DeviceBusy:
        return CameraControlEditor::tr("câmera ocupada por outra escrita");
    case shared_kernel::ErrorCode::InvalidArgument:
        return CameraControlEditor::tr("valor recusado");
    case shared_kernel::ErrorCode::LeaseExpired:
    case shared_kernel::ErrorCode::LeaseRevoked:
        return CameraControlEditor::tr("escrita cancelada");
    case shared_kernel::ErrorCode::BackendUnavailable:
        return CameraControlEditor::tr("câmera indisponível");
    case shared_kernel::ErrorCode::MalformedData:
        break;
    }
    return CameraControlEditor::tr("falha ao gravar");
}

} // namespace

CameraControlEditor::CameraControlEditor(const SetCameraControl& setCameraControl, QObject* parent)
    : QObject(parent), setCameraControl_(&setCameraControl)
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &CameraControlEditor::onFinished);
}

CameraControlEditor::~CameraControlEditor()
{
    // A escrita não é cancelável: espera terminar, pois o caso de uso deve continuar válido até lá.
    pending_.reset();
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
}

void CameraControlEditor::setValue(quint32 id, qint64 value)
{
    submit({.id = id, .restoreDefault = false, .value = value});
}

void CameraControlEditor::restoreDefault(quint32 id)
{
    submit({.id = id, .restoreDefault = true, .value = 0});
}

void CameraControlEditor::submit(Request request)
{
    if (running_) {
        pending_ = request; // só o último pedido vale
        return;
    }
    start(request);
}

void CameraControlEditor::start(Request request)
{
    running_ = true;
    const SetCameraControl& useCase = *setCameraControl_;
    watcher_.setFuture(QtConcurrent::run([&useCase, request] {
        return request.restoreDefault ? useCase.restoreDefault(request.id)
                                      : useCase.setValue(request.id, request.value);
    }));
    emit editorChanged();
}

void CameraControlEditor::onFinished()
{
    const auto outcome = watcher_.result();
    if (outcome.has_value()) {
        status_ = successText(*outcome);
    } else {
        qWarning("camera: escrita de controle falhou (código %d): %s", static_cast<int>(outcome.error().code),
                 outcome.error().detail.c_str());
        status_ = failureText(outcome.error().code);
    }
    running_ = false;
    if (const auto next = std::exchange(pending_, std::nullopt); next.has_value()) {
        start(*next);
    }
    emit editorChanged();
    emit controlWritten();
}

} // namespace ghub::camera
