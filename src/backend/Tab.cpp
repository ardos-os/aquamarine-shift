#include "aquamarine/backend/Tab.hpp"

#include "aquamarine/allocator/Swapchain.hpp"
#include "aquamarine/backend/Backend.hpp"
#include "aquamarine/buffer/Buffer.hpp"
#include "aquamarine/input/Input.hpp"
#include "aquamarine/output/Output.hpp"
#include <algorithm>
#include <cstdio>
#include <drm_fourcc.h>
#include <format>
#include <optional>
#include <ctime>
#include <unistd.h>

using namespace Aquamarine;
using namespace Hyprutils::Memory;
using namespace Hyprutils::Math;
#define SP CSharedPointer

namespace {

class CTabKeyboard : public IKeyboard {
  public:
    const std::string& getName() override {
        static const std::string name = "tab-keyboard";
        return name;
    }

    libinput_device* getLibinputHandle() override {
        return nullptr;
    }

    void updateLEDs(uint32_t leds) override {
        (void)leds;
    }
};

class CTabPointer : public IPointer {
  public:
    const std::string& getName() override {
        static const std::string name = "tab-pointer";
        return name;
    }

    libinput_device* getLibinputHandle() override {
        return nullptr;
    }
};

class CTabTouch : public ITouch {
  public:
    const std::string& getName() override {
        static const std::string name = "tab-touch";
        return name;
    }

    libinput_device* getLibinputHandle() override {
        return nullptr;
    }
};

class CTabTablet : public ITablet {
  public:
    const std::string& getName() override {
        static const std::string name = "tab-tablet";
        return name;
    }

    libinput_device* getLibinputHandle() override {
        return nullptr;
    }
};

class CTabTabletPad : public ITabletPad {
  public:
    const std::string& getName() override {
        static const std::string name = "tab-tablet-pad";
        return name;
    }

    libinput_device* getLibinputHandle() override {
        return nullptr;
    }
};

class CTabSwitch : public ISwitch {
  public:
    const std::string& getName() override {
        static const std::string name = "tab-switch";
        return name;
    }

    libinput_device* getLibinputHandle() override {
        return nullptr;
    }
};

class CTabTabletTool : public ITabletTool {
  public:
    const std::string& getName() override {
        static const std::string name = "tab-tablet-tool";
        return name;
    }

    libinput_device* getLibinputHandle() override {
        return nullptr;
    }
};

class CTabBuffer : public IBuffer {
  public:
    explicit CTabBuffer(const TabFrameTarget& target_) : target(target_) {
        ;
    }

    ~CTabBuffer() override {
        if (target.dmabuf.fd >= 0)
            close(target.dmabuf.fd);
    }

    eBufferType type() override {
        return eBufferType::BUFFER_TYPE_DMABUF;
    }

    eBufferCapability caps() override {
        return eBufferCapability::BUFFER_CAPABILITY_NONE;
    }

    void update(const CRegion& damage) override {
        (void)damage;
    }

    bool good() override {
        return target.dmabuf.fd >= 0;
    }

    SDMABUFAttrs dmabuf() override {
        SDMABUFAttrs attrs;
        attrs.success          = target.dmabuf.fd >= 0;
        attrs.size             = {double(target.width), double(target.height)};
        attrs.format           = target.dmabuf.fourcc;
        attrs.modifier         = DRM_FORMAT_MOD_INVALID;
        attrs.strides[0]       = target.dmabuf.stride;
        attrs.offsets[0]       = target.dmabuf.offset;
        attrs.fds[0]           = target.dmabuf.fd;
        attrs.planes           = 1;
        return attrs;
    }

    bool isSynchronous() override {
        return false;
    }

  private:
    TabFrameTarget target;
};

class CTabSwapchain : public ISwapchain {
  public:
    CTabSwapchain(const TabMonitorInfo& monitor_info, TabClientHandle* handle)
        : client(handle), monitorID(monitor_info.id) {
        options.length = 2;
        options.size   = {monitor_info.width, monitor_info.height};
        options.format = DRM_FORMAT_ARGB8888;
    }

    bool reconfigure(const SSwapchainOptions& options_) override {
        options = options_;
        return true;
    }

    CSharedPointer<IBuffer> next(int* age) override {
        if (age)
            *age = 0;
        if (!client)
            return nullptr;

        TabFrameTarget target {};
        const auto     res = tab_client_acquire_frame(client, monitorID.c_str(), &target);
        if (res != TAB_ACQUIRE_OK)
            return nullptr;

        pending = target.dmabuf.fd >= 0;
        return CSharedPointer<IBuffer>(new CTabBuffer(target));
    }

    const SSwapchainOptions& currentOptions() override {
        return options;
    }

    void rollback() override {
        pending = false;
    }

    bool takePending() {
        bool had = pending;
        pending  = false;
        return had;
    }

  private:
    TabClientHandle*  client = nullptr;
    std::string       monitorID;
    SSwapchainOptions options;
    bool              pending = false;

    friend class CTabOutput;
};

} // namespace

CTabOutput::CTabOutput(const TabMonitorInfo& monitor_info, CWeakPointer<CTabBackend> backend_)
    : backend(backend_), monitorID(monitor_info.id) {
    name          = monitor_info.name;
    physicalSize  = {double(monitor_info.width), double(monitor_info.height)};
    refreshRateHz = monitor_info.refresh_rate > 0 ? monitor_info.refresh_rate : 60;
    refreshIntervalNs = refreshRateHz > 0 ? (int)(1000000000LL / refreshRateHz) : 0;

    const unsigned refreshmHz = std::max(1, refreshRateHz) * 1000U;
    auto mode = CSharedPointer<SOutputMode>(
        new SOutputMode({.pixelSize = {double(monitor_info.width), double(monitor_info.height)}, .refreshRate = refreshmHz, .preferred = true}));
    modes.emplace_back(mode);
    state->setMode(mode);

    swapchain = CSharedPointer<ISwapchain>(new CTabSwapchain(monitor_info, backend.lock()->ensureClient()));
}

CTabOutput::~CTabOutput() {
    events.destroy.emit();
}

bool CTabOutput::commit() {
    events.commit.emit();
    state->onCommit();
    needsFrame = false;
    auto be = backend.lock();
    if (!be)
        return true;

    auto sc = dynamicPointerCast<CTabSwapchain>(swapchain);
    if (!sc)
        return true;

    if (auto client = be->ensureClient()) {
        const bool sent = sc->takePending();
        if (sent)
            tab_client_swap_buffers(client, monitorID.c_str());
        if (sent)
            awaitingFrameDone = true;
    }

    return true;
}

bool CTabOutput::test() {
    return true;
}

SP<IBackendImplementation> CTabOutput::getBackend() {
    return backend.lock();
}

void CTabOutput::scheduleFrame(const scheduleFrameReason) {
    needsFrame = true;
    if (awaitingFrameDone || frameEventScheduled)
        return;
    frameEventScheduled = true;

    auto be = backend.lock();
    if (!be)
        return;
    if (auto core = be->backend.lock()) {
        if (!frameIdle)
            return;
        core->addIdleEvent(frameIdle);
    }
}

bool CTabOutput::destroy() {
    events.destroy.emit();
    if (auto be = backend.lock())
        std::erase_if(be->outputs, [this](const auto& other) { return other.get() == this; });
    return true;
}

std::vector<SDRMFormat> CTabOutput::getRenderFormats() {
    if (auto be = backend.lock())
        return be->getRenderFormats();
    return {};
}

CTabBackend::CTabBackend(CSharedPointer<CBackend> backend_) : backend(backend_) {
    ;
}

CTabBackend::~CTabBackend() {
    if (client) {
        tab_client_disconnect(client);
        client = nullptr;
    }
}

TabClientHandle* CTabBackend::ensureClient() {
    if (client)
        return client;
    const char* token = std::getenv("SHIFT_SESSION_TOKEN");
    client            = tab_client_connect_default(token);
    if (!client && backend.lock())
        backend.lock()->log(AQ_LOG_ERROR, "tab backend: failed to connect to Shift session");
    return client;
}

CTabOutput* CTabBackend::findOutputByID(const std::string& id) {
    for (auto& output : outputs)
        if (output->monitorID == id)
            return output.get();
    return nullptr;
}

eBackendType CTabBackend::type() {
    return eBackendType::AQ_BACKEND_TAB;
}

bool CTabBackend::start() {
    if (!ensureClient())
        return false;

    auto core = backend.lock();
    if (core)
        core->log(AQ_LOG_DEBUG, "tab backend: connected to Shift");

    const size_t count = tab_client_get_monitor_count(client);
    for (size_t i = 0; i < count; ++i) {
        char* id = tab_client_get_monitor_id(client, i);
        if (!id)
            continue;
        auto info = tab_client_get_monitor_info(client, id);
        createOutput(info);
        tab_client_free_monitor_info(&info);
        tab_client_string_free(id);
    }

    if (core)
        core->events.pollFDsChanged.emit();

    return true;
}

std::vector<CSharedPointer<SPollFD>> CTabBackend::pollFDs() {
    if (!client)
        return {};
    const int fd = tab_client_get_socket_fd(client);
    if (fd < 0)
        return {};
    return {CSharedPointer<SPollFD>(new SPollFD{.fd = fd, .onSignal = [this]() { dispatchEvents(); }})};
}

int CTabBackend::drmFD() {
    return client ? tab_client_drm_fd(client) : -1;
}

int CTabBackend::drmRenderNodeFD() {
    return drmFD();
}

bool CTabBackend::dispatchEvents() {
    if (!client)
        return true;

    tab_client_poll_events(client);

    TabEvent event {};
    while (tab_client_next_event(client, &event)) {
        switch (event.event_type) {
            case TAB_EVENT_FRAME_DONE: {
                if (event.data.frame_done) {
                    std::string id(event.data.frame_done);
                    if (auto output = findOutputByID(id)) {
                        if (!output->awaitingFrameDone) {
                            break;
                        }
                        timespec now {};
                        clock_gettime(CLOCK_MONOTONIC, &now);
                        output->lastPresentTime = now;
                        output->events.present.emit(IOutput::SPresentEvent{
                            .presented = true,
                            .when      = &output->lastPresentTime,
                            .seq       = ++output->presentSeq,
                            .refresh   = output->refreshIntervalNs,
                            .flags     = IOutput::AQ_OUTPUT_PRESENT_VSYNC,
                        });
                        output->awaitingFrameDone = false;
                        if (output->needsFrame && !output->frameEventScheduled) {
                            output->scheduleFrame(IOutput::AQ_SCHEDULE_NEEDS_FRAME);
                        }
                    } else if (auto core = backend.lock()) {
                        core->log(AQ_LOG_WARNING, std::format("tab frame_done for unknown monitor {}", id));
                    }
                } else if (auto core = backend.lock()) {
                    core->log(AQ_LOG_WARNING, "tab frame_done event with null monitor id");
                }
                break;
            }
            case TAB_EVENT_MONITOR_ADDED: {
                createOutput(event.data.monitor_added);
                break;
            }
            case TAB_EVENT_MONITOR_REMOVED: {
                if (event.data.monitor_removed) {
                    std::string id(event.data.monitor_removed);
                    std::erase_if(outputs, [&](const auto& other) {
                        if (other->monitorID == id) {
                            other->destroy();
                            return true;
                        }
                        return false;
                    });
                }
                break;
            }
            case TAB_EVENT_INPUT: {
                bool pointerDirty = false;
                bool touchDirty   = false;
                handleInput(&event.data.input, pointerDirty, touchDirty);
                if (pointerDirty && pointer)
                    pointer->events.frame.emit();
                if (touchDirty && touch)
                    touch->events.frame.emit();
                break;
            }
            default: {
                if (auto core = backend.lock())
                    core->log(AQ_LOG_DEBUG,
                              std::format("tab backend: unhandled event {}", (int)event.event_type));
                break;
            }
        }
        tab_client_free_event_strings(&event);
    }
    return true;
}

uint32_t CTabBackend::capabilities() {
    return 0;
}

bool CTabBackend::setCursor(SP<IBuffer>, const Vector2D&) {
    return false;
}

void CTabBackend::onReady() {
    ;
}

std::vector<SDRMFormat> CTabBackend::getRenderFormats() {
    if (auto core = backend.lock()) {
        for (const auto& impl : core->getImplementations()) {
            if (impl->type() != AQ_BACKEND_DRM || impl->getRenderableFormats().empty())
                continue;
            return impl->getRenderableFormats();
        }
    }

    return {SDRMFormat{.drmFormat = DRM_FORMAT_XRGB8888, .modifiers = {DRM_FORMAT_INVALID}},
            SDRMFormat{.drmFormat = DRM_FORMAT_ARGB8888, .modifiers = {DRM_FORMAT_INVALID}}};
}

std::vector<SDRMFormat> CTabBackend::getCursorFormats() {
    return {};
}

bool CTabBackend::createOutput(const TabMonitorInfo& monitor_info) {
    auto output = CSharedPointer<CTabOutput>(new CTabOutput(monitor_info, self));
    output->self = output;
    output->frameIdle = makeShared<std::function<void(void)>>([w = output->self]() {
        if (auto o = w.lock()) {
            o->frameEventScheduled = false;
            if (o->awaitingFrameDone) {
                return;
            }
            o->events.frame.emit();
        }
    });
    outputs.emplace_back(output);
    if (auto core = backend.lock())
        core->events.newOutput.emit(output);
    return true;
}

CSharedPointer<IAllocator> CTabBackend::preferredAllocator() {
    return nullptr;
}

std::vector<CSharedPointer<IAllocator>> CTabBackend::getAllocators() {
    return {};
}

CWeakPointer<IBackendImplementation> CTabBackend::getPrimary() {
    return self;
}

void CTabBackend::handleInput(TabInputEvent* event, bool& pointerDirty, bool& touchDirty) {
    auto core = backend.lock();
    switch (event->kind) {
        case TAB_INPUT_KIND_KEY: {
            if (!keyboard) {
                keyboard = CSharedPointer<IKeyboard>(new CTabKeyboard());
                if (core)
                    core->events.newKeyboard.emit(keyboard);
            }
            auto& key = event->data.key;
            keyboard->events.key.emit(IKeyboard::SKeyEvent{
                .timeMs  = (uint32_t)(key.time_usec / 1000),
                .key     = key.key,
                .pressed = key.state == TAB_KEY_PRESSED,
            });
            break;
        }
        case TAB_INPUT_KIND_POINTER_BUTTON: {
            if (!pointer) {
                pointer = CSharedPointer<IPointer>(new CTabPointer());
                if (core)
                    core->events.newPointer.emit(pointer);
            }
            auto& button = event->data.pointer_button;
            pointer->events.button.emit(IPointer::SButtonEvent{
                .timeMs  = (uint32_t)(button.time_usec / 1000),
                .button  = button.button,
                .pressed = button.state == TAB_BUTTON_PRESSED,
            });
            pointerDirty = true;
            break;
        }
        case TAB_INPUT_KIND_POINTER_MOTION: {
            if (!pointer) {
                pointer = CSharedPointer<IPointer>(new CTabPointer());
                if (core)
                    core->events.newPointer.emit(pointer);
            }
            auto& motion = event->data.pointer_motion;
            pointer->events.move.emit(IPointer::SMoveEvent{
                .timeMs = (uint32_t)(motion.time_usec / 1000),
                .delta  = {motion.dx, motion.dy},
                .unaccel = {motion.unaccel_dx, motion.unaccel_dy},
            });
            pointerDirty = true;
            break;
        }
        case TAB_INPUT_KIND_POINTER_MOTION_ABSOLUTE: {
            if (!pointer) {
                pointer = CSharedPointer<IPointer>(new CTabPointer());
                if (core)
                    core->events.newPointer.emit(pointer);
            }
            auto& abs = event->data.pointer_motion_absolute;
            pointer->events.warp.emit(IPointer::SWarpEvent{
                .timeMs   = (uint32_t)(abs.time_usec / 1000),
                .absolute = {abs.x, abs.y},
            });
            pointerDirty = true;
            break;
        }
        case TAB_INPUT_KIND_POINTER_AXIS: {
            if (!pointer) {
                pointer = CSharedPointer<IPointer>(new CTabPointer());
                if (core)
                    core->events.newPointer.emit(pointer);
            }
            auto& axis = event->data.pointer_axis;
            IPointer::SAxisEvent ev;
            ev.timeMs = (uint32_t)(axis.time_usec / 1000);
            ev.axis   = axis.orientation == TAB_AXIS_VERTICAL ? IPointer::AQ_POINTER_AXIS_VERTICAL : IPointer::AQ_POINTER_AXIS_HORIZONTAL;
            switch (axis.source) {
                case TAB_AXIS_SOURCE_WHEEL: ev.source = IPointer::AQ_POINTER_AXIS_SOURCE_WHEEL; break;
                case TAB_AXIS_SOURCE_FINGER: ev.source = IPointer::AQ_POINTER_AXIS_SOURCE_FINGER; break;
                case TAB_AXIS_SOURCE_CONTINUOUS: ev.source = IPointer::AQ_POINTER_AXIS_SOURCE_CONTINUOUS; break;
                case TAB_AXIS_SOURCE_WHEEL_TILT: ev.source = IPointer::AQ_POINTER_AXIS_SOURCE_TILT; break;
            }
            ev.delta    = axis.delta;
            ev.discrete = axis.delta_discrete;
            pointer->events.axis.emit(ev);
            pointerDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TOUCH_DOWN: {
            if (!touch) {
                touch = CSharedPointer<ITouch>(new CTabTouch());
                if (core)
                    core->events.newTouch.emit(touch);
            }
            auto& t = event->data.touch_down;
            touch->events.down.emit(ITouch::SDownEvent{
                .timeMs  = (uint32_t)(t.time_usec / 1000),
                .touchID = t.contact.id,
                .pos     = {t.contact.x, t.contact.y},
            });
            touchDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TOUCH_UP: {
            if (!touch) {
                touch = CSharedPointer<ITouch>(new CTabTouch());
                if (core)
                    core->events.newTouch.emit(touch);
            }
            auto& t = event->data.touch_up;
            touch->events.up.emit(ITouch::SUpEvent{
                .timeMs  = (uint32_t)(t.time_usec / 1000),
                .touchID = t.contact_id,
            });
            touchDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TOUCH_MOTION: {
            if (!touch) {
                touch = CSharedPointer<ITouch>(new CTabTouch());
                if (core)
                    core->events.newTouch.emit(touch);
            }
            auto& t = event->data.touch_motion;
            touch->events.move.emit(ITouch::SMotionEvent{
                .timeMs  = (uint32_t)(t.time_usec / 1000),
                .touchID = t.contact.id,
                .pos     = {t.contact.x, t.contact.y},
            });
            touchDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TOUCH_FRAME: {
            if (touch)
                touch->events.frame.emit();
            touchDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TOUCH_CANCEL: {
            if (!touch) {
                touch = CSharedPointer<ITouch>(new CTabTouch());
                if (core)
                    core->events.newTouch.emit(touch);
            }
            auto& cancel = event->data.touch_cancel;
            touch->events.cancel.emit(ITouch::SCancelEvent{
                .timeMs  = (uint32_t)(cancel.time_usec / 1000),
                .touchID = -1,
            });
            touchDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TABLET_TOOL_AXIS: {
            if (!tablet) {
                tablet = CSharedPointer<ITablet>(new CTabTablet());
                if (core)
                    core->events.newTablet.emit(tablet);
            }
            auto& axis = event->data.tablet_tool_axis;
            auto  tool = CSharedPointer<ITabletTool>(new CTabTabletTool());
            tablet->events.axis.emit(ITablet::SAxisEvent{
                .tool     = tool,
                .timeMs   = (uint32_t)(axis.time_usec / 1000),
                .absolute = {axis.axes.x, axis.axes.y},
                .tilt     = {axis.axes.tilt_x, axis.axes.tilt_y},
                .pressure = axis.axes.pressure,
                .distance = axis.axes.distance,
                .rotation = axis.axes.rotation,
            });
            break;
        }
        case TAB_INPUT_KIND_TABLET_TOOL_PROXIMITY: {
            if (!tablet) {
                tablet = CSharedPointer<ITablet>(new CTabTablet());
                if (core)
                    core->events.newTablet.emit(tablet);
            }
            auto& proximity = event->data.tablet_tool_proximity;
            auto  tool      = CSharedPointer<ITabletTool>(new CTabTabletTool());
            tablet->events.proximity.emit(ITablet::SProximityEvent{
                .tool   = tool,
                .timeMs = (uint32_t)(proximity.time_usec / 1000),
                .in     = proximity.in_proximity,
            });
            break;
        }
        case TAB_INPUT_KIND_TABLET_TOOL_TIP: {
            if (!tablet) {
                tablet = CSharedPointer<ITablet>(new CTabTablet());
                if (core)
                    core->events.newTablet.emit(tablet);
            }
            auto& tip  = event->data.tablet_tool_tip;
            auto  tool = CSharedPointer<ITabletTool>(new CTabTabletTool());
            tablet->events.tip.emit(ITablet::STipEvent{
                .tool   = tool,
                .timeMs = (uint32_t)(tip.time_usec / 1000),
                .down   = tip.state == TAB_TIP_DOWN,
            });
            break;
        }
        case TAB_INPUT_KIND_TABLET_TOOL_BUTTON: {
            if (!tablet) {
                tablet = CSharedPointer<ITablet>(new CTabTablet());
                if (core)
                    core->events.newTablet.emit(tablet);
            }
            auto& button = event->data.tablet_tool_button;
            auto  tool   = CSharedPointer<ITabletTool>(new CTabTabletTool());
            tablet->events.button.emit(ITablet::SButtonEvent{
                .tool   = tool,
                .timeMs = (uint32_t)(button.time_usec / 1000),
                .button = button.button,
                .down   = button.state == TAB_BUTTON_PRESSED,
            });
            break;
        }
        case TAB_INPUT_KIND_TABLET_PAD_BUTTON: {
            if (!tabletPad) {
                tabletPad = CSharedPointer<ITabletPad>(new CTabTabletPad());
                if (core)
                    core->events.newTabletPad.emit(tabletPad);
            }
            auto& b = event->data.tablet_pad_button;
            tabletPad->events.button.emit(ITabletPad::SButtonEvent{
                .timeMs = (uint32_t)(b.time_usec / 1000),
                .button = b.button,
                .down   = b.state == TAB_BUTTON_PRESSED,
                .mode   = 0,
                .group  = 0,
            });
            break;
        }
        case TAB_INPUT_KIND_TABLET_PAD_RING: {
            if (!tabletPad) {
                tabletPad = CSharedPointer<ITabletPad>(new CTabTabletPad());
                if (core)
                    core->events.newTabletPad.emit(tabletPad);
            }
            auto& r = event->data.tablet_pad_ring;
            tabletPad->events.ring.emit(ITabletPad::SRingEvent{
                .timeMs = (uint32_t)(r.time_usec / 1000),
                .source = ITabletPad::AQ_TABLET_PAD_RING_SOURCE_FINGER,
                .ring   = (uint16_t)r.ring,
                .pos    = r.position,
                .mode   = 0,
            });
            break;
        }
        case TAB_INPUT_KIND_TABLET_PAD_STRIP: {
            if (!tabletPad) {
                tabletPad = CSharedPointer<ITabletPad>(new CTabTabletPad());
                if (core)
                    core->events.newTabletPad.emit(tabletPad);
            }
            auto& s = event->data.tablet_pad_strip;
            tabletPad->events.strip.emit(ITabletPad::SStripEvent{
                .timeMs = (uint32_t)(s.time_usec / 1000),
                .source = ITabletPad::AQ_TABLET_PAD_STRIP_SOURCE_FINGER,
                .strip  = (uint16_t)s.strip,
                .pos    = s.position,
                .mode   = 0,
            });
            break;
        }
        case TAB_INPUT_KIND_SWITCH_TOGGLE: {
            if (!switchDev) {
                switchDev = CSharedPointer<ISwitch>(new CTabSwitch());
                if (core)
                    core->events.newSwitch.emit(switchDev);
            }
            auto& sw = event->data.switch_toggle;
            switchDev->events.fire.emit(ISwitch::SFireEvent{
                .timeMs = (uint32_t)(sw.time_usec / 1000),
                .type   = sw.switch_type == TAB_SWITCH_LID ? ISwitch::AQ_SWITCH_TYPE_LID : ISwitch::AQ_SWITCH_TYPE_TABLET_MODE,
                .enable = sw.state == TAB_SWITCH_ON,
            });
            break;
        }
        default: {
            if (core)
                core->log(AQ_LOG_DEBUG, std::format("tab backend: unhandled input type {}", (int)event->kind));
            break;
        }
    }
}
