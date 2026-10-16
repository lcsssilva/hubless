#include "modules/lighting/application/SyncColor.hpp"

namespace ghub::lighting::application {

namespace {

SyncOutcome fromResult(SyncDevice device, const shared_kernel::Result<void>& result)
{
    if (result) {
        return {.device = device, .status = SyncStatus::Applied, .detail = {}};
    }
    return {.device = device, .status = SyncStatus::Failed, .detail = result.error().detail};
}

} // namespace

std::vector<SyncOutcome> SyncColor::apply(const SyncRequest& request) const
{
    std::vector<SyncOutcome> outcomes;
    if (request.keyboard) {
        if (keyboard_->ready()) {
            outcomes.push_back(fromResult(SyncDevice::Keyboard, keyboard_->applyFixed(request.color)));
        } else {
            outcomes.push_back({.device = SyncDevice::Keyboard,
                                .status = SyncStatus::Skipped,
                                .detail = "ligue o modo ao vivo do G915 para sincronizar o teclado"});
        }
    }
    if (request.headset) {
        outcomes.push_back(fromResult(SyncDevice::Headset, headset_->applyFixed(request.color)));
    }
    if (request.mouse) {
        if (request.confirmPersistentMouse) {
            outcomes.push_back(fromResult(SyncDevice::Mouse, mouse_->applyFixed(request.color)));
        } else {
            outcomes.push_back({.device = SyncDevice::Mouse,
                                .status = SyncStatus::Skipped,
                                .detail = "o LED do G703 é gravado no mouse (persistente): confirme para incluí-lo"});
        }
    }
    return outcomes;
}

} // namespace ghub::lighting::application
