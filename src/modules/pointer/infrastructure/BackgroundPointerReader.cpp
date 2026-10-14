#include "modules/pointer/infrastructure/BackgroundPointerReader.hpp"

#include <QtConcurrent>

namespace ghub::pointer {

BackgroundPointerReader::BackgroundPointerReader(const ReadPointerConfiguration& useCase, QObject* parent)
    : QObject(parent), useCase_(useCase)
{
    connect(&watcher_, &QFutureWatcher<PointerStatus>::finished, this, &BackgroundPointerReader::onFinished);
}

BackgroundPointerReader::~BackgroundPointerReader()
{
    // A leitura não é cancelável: espera terminar, pois o caso de uso deve continuar válido até lá.
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
}

void BackgroundPointerReader::start()
{
    if (running_) {
        return;
    }
    running_ = true;
    const ReadPointerConfiguration& useCase = useCase_;
    watcher_.setFuture(QtConcurrent::run([&useCase] { return useCase.execute(); }));
}

void BackgroundPointerReader::onFinished()
{
    last_ = watcher_.result();
    running_ = false;
    emit readFinished();
}

} // namespace ghub::pointer
