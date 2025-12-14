#include <aquamarine/backend/Tab.hpp>

using namespace Aquamarine;
using namespace Hyprutils::Memory;
using namespace Hyprutils::Math;
#define SP CSharedPointer

Aquamarine::CTabOutput::CTabOutput(const std::string& name_, CWeakPointer<CTabBackend> backend_) : backend(backend_) {
    name = name_;

    framecb = makeShared<std::function<void()>>([this]() {
        frameScheduled = false;
        events.frame.emit();
    });
}

Aquamarine::CTabOutput::~CTabOutput() {
    backend.lock()->backend.lock()->removeIdleEvent(framecb);
    events.destroy.emit();
}

bool Aquamarine::CTabOutput::commit() {
    events.commit.emit();
    state->onCommit();
    needsFrame = false;
    events.present.emit(IOutput::SPresentEvent{.presented = true});
    return true;
}

bool Aquamarine::CTabOutput::test() {
    return true;
}

SP<IBackendImplementation> Aquamarine::CTabOutput::getBackend() {
    return backend.lock();
}

void Aquamarine::CTabOutput::scheduleFrame(const scheduleFrameReason reason) {
    needsFrame = true;

    if (frameScheduled)
        return;

    frameScheduled = true;
    backend.lock()->backend.lock()->addIdleEvent(framecb);
}

bool Aquamarine::CTabOutput::destroy() {
    events.destroy.emit();
    std::erase(backend.lock()->outputs, self.lock());
    return true;
}

std::vector<SDRMFormat> Aquamarine::CTabOutput::getRenderFormats() {
    if (auto be = backend.lock())
        return be->getRenderFormats();
    return {};
}

Aquamarine::CTabBackend::~CTabBackend() {
    ;
}

Aquamarine::CTabBackend::CTabBackend(SP<CBackend> backend_) : backend(backend_) {
    ;
}

eBackendType Aquamarine::CTabBackend::type() {
    return eBackendType::AQ_BACKEND_TAB;
}

bool Aquamarine::CTabBackend::start() {
    return true;
}

std::vector<SP<SPollFD>> Aquamarine::CTabBackend::pollFDs() {
    return {};
}

int Aquamarine::CTabBackend::drmFD() {
    return -1;
}

int Aquamarine::CTabBackend::drmRenderNodeFD() {
    return -1;
}

bool Aquamarine::CTabBackend::dispatchEvents() {
    return true;
}

uint32_t Aquamarine::CTabBackend::capabilities() {
    return 0;
}

bool Aquamarine::CTabBackend::setCursor(SP<IBuffer> buffer, const Hyprutils::Math::Vector2D& hotspot) {
    return false;
}

void Aquamarine::CTabBackend::onReady() {
    ;
}

std::vector<SDRMFormat> Aquamarine::CTabBackend::getRenderFormats() {
    for (const auto& impl : backend.lock()->getImplementations()) {
        if (impl->type() == AQ_BACKEND_TAB)
            continue;
        if (impl->getRenderableFormats().empty())
            continue;
        return impl->getRenderableFormats();
    }

    return {};
}

std::vector<SDRMFormat> Aquamarine::CTabBackend::getCursorFormats() {
    return {};
}

bool Aquamarine::CTabBackend::createOutput(const std::string& name) {
    auto output = SP<CTabOutput>(new CTabOutput(name.empty() ? std::format("TAB-{}", outputIDCounter++) : name, self));
    output->self = output;
    outputs.emplace_back(output);

    backend.lock()->events.newOutput.emit(output);

    return true;
}

SP<IAllocator> Aquamarine::CTabBackend::preferredAllocator() {
    return backend.lock()->primaryAllocator;
}

std::vector<SP<IAllocator>> Aquamarine::CTabBackend::getAllocators() {
    return {backend.lock()->primaryAllocator};
}

CWeakPointer<IBackendImplementation> Aquamarine::CTabBackend::getPrimary() {
    return {};
}