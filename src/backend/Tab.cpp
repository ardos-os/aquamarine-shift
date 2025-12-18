#include "Shared.hpp"
#include "aquamarine/allocator/Swapchain.hpp"
#include "aquamarine/backend/Misc.hpp"
#include "aquamarine/buffer/Buffer.hpp"
#include "aquamarine/output/Output.hpp"
#include <aquamarine/backend/Tab.hpp>
#include <aquamarine/backend/Backend.hpp>
#include <aquamarine/input/Input.hpp>
#include <cstdlib>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <sys/timerfd.h>

extern "C" {
#include <tab_client.h>
}

using namespace Aquamarine;
using namespace Hyprutils::Memory;
using namespace Hyprutils::Math;
#define SP CSharedPointer

class CTabKeyboard : public IKeyboard {
  public:
    CTabKeyboard() {
        ;
    }

    virtual const std::string& getName() {
        static const std::string name = "tab-keyboard";
        return name;
    }

    virtual libinput_device* getLibinputHandle() {
        return nullptr;
    }

    virtual void updateLEDs(uint32_t leds) {
        ;
    }
};

class CTabPointer : public IPointer {
  public:
    CTabPointer() {
        ;
    }

    virtual const std::string& getName() {
        static const std::string name = "tab-pointer";
        return name;
    }

    virtual libinput_device* getLibinputHandle() {
        return nullptr;
    }
};

class CTabTouch : public ITouch {
  public:
    CTabTouch() {
        ;
    }

    virtual const std::string& getName() {
        static const std::string name = "tab-touch";
        return name;
    }

    virtual libinput_device* getLibinputHandle() {
        return nullptr;
    }
};

class CTabTabletTool : public ITabletTool {
  public:
    CTabTabletTool() {
        ;
    }

    virtual const std::string& getName() {
        static const std::string name = "tab-tablet-tool";
        return name;
    }

    virtual libinput_device* getLibinputHandle() {
        return nullptr;
    }
};

class CTabTablet : public ITablet {
  public:
    CTabTablet() {
        ;
    }

    virtual const std::string& getName() {
        static const std::string name = "tab-tablet";
        return name;
    }

    virtual libinput_device* getLibinputHandle() {
        return nullptr;
    }
};

class CTabTabletPad : public ITabletPad {
  public:
    CTabTabletPad() {
        ;
    }

    virtual const std::string& getName() {
        static const std::string name = "tab-tablet-pad";
        return name;
    }

    virtual libinput_device* getLibinputHandle() {
        return nullptr;
    }
};

class CTabSwitch : public ISwitch {
  public:
    CTabSwitch() {
        ;
    }

    virtual const std::string& getName() {
        static const std::string name = "tab-switch";
        return name;
    }

    virtual libinput_device* getLibinputHandle() {
        return nullptr;
    }
};

class CTabBuffer : public IBuffer {
    private:
        TabFrameTarget target;
    public:
        CTabBuffer(const TabFrameTarget& target_) : target(target_) {
            ;
        }
        virtual ~CTabBuffer() {
            ;
        }
        virtual eBufferType type() {
            return eBufferType::BUFFER_TYPE_DMABUF;
        }
        virtual eBufferCapability caps() {
            return eBufferCapability::BUFFER_CAPABILITY_NONE;
        }
        virtual void update(const Hyprutils::Math::CRegion& damage) {
            ;
        }
        virtual bool good() {
            return true;
        }
        virtual SDMABUFAttrs dmabuf() {
            SDMABUFAttrs attrs;
            attrs.success = true;
            attrs.size = Hyprutils::Math::Vector2D{(double)target.width, (double)target.height};
            attrs.format = target.dmabuf.fourcc;
            attrs.modifier = DRM_FORMAT_MOD_INVALID;
            attrs.strides[0] = target.dmabuf.stride;
            attrs.offsets[0] = target.dmabuf.offset;
            attrs.fds[0] = target.dmabuf.fd;
            return attrs;
        }
        virtual bool isSynchronous() {
            return false;
        }
};

class CTabSwapchain : public ISwapchain {
    public:
        SSwapchainOptions options;
        TabClientHandle* tab_client;
        std::string monitor_id;
        virtual bool                                                 reconfigure(const SSwapchainOptions& options_) {
            
            // ignore
            return true;
        };

        virtual CSharedPointer<IBuffer> next(int* age) {
            TabFrameTarget target;

            auto res = tab_client_acquire_frame(tab_client, monitor_id.c_str(), &target);
            if (res != TAB_ACQUIRE_OK)
                return nullptr;

            return CSharedPointer<IBuffer>(new CTabBuffer(target));
        }

        virtual const SSwapchainOptions&                             currentOptions() {
            return options;
        };
        

        // rolls the buffers back, marking the last consumed as the next valid.
        // useful if e.g. a commit fails and we don't wanna write to the previous buffer that is
        // in use.
        virtual void rollback() {

        };
        CTabSwapchain(const TabMonitorInfo& monitor_info, TabClientHandle* tab_client) {
            TabFrameTarget target;
            tab_client_acquire_frame(tab_client, monitor_id.c_str(), &target);
            options = SSwapchainOptions {
                .length = 2,
                .size = {monitor_info.width, monitor_info.height},
                .format = target.dmabuf.fourcc,
                .scanout = false,
                .cursor = false,
                .multigpu = false
            };
            this->tab_client = tab_client;
            this->monitor_id = std::string(monitor_info.id);
        };
        ~CTabSwapchain() {
            ;
        }
};
Aquamarine::CTabOutput::CTabOutput(const TabMonitorInfo& monitor_info, Hyprutils::Memory::CWeakPointer<CTabBackend> backend_) : backend(backend_) {
    this->name = std::string(monitor_info.name);
    this->physicalSize = {(double)monitor_info.width, (double)monitor_info.height};
    this->swapchain = Hyprutils::Memory::CSharedPointer<ISwapchain>(new CTabSwapchain(monitor_info, backend.lock()->m_pClient));
    this->monitor_id = std::string(monitor_info.id);
    this->modes.emplace_back(Hyprutils::Memory::CSharedPointer<SOutputMode>(new SOutputMode(Vector2D{(double)monitor_info.width, (double)monitor_info.height}, 60, true)));

}

Aquamarine::CTabOutput::~CTabOutput() {
    events.destroy.emit();
}


bool Aquamarine::CTabOutput::commit() {
    events.commit.emit();
    state->onCommit();
    needsFrame = false;
    tab_client_swap_buffers(backend->m_pClient, this->monitor_id.c_str());
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
}

bool Aquamarine::CTabOutput::destroy() {
    events.destroy.emit();
    std::erase_if(backend.lock()->outputs, [this](const auto& other) { return other.get() == this; });
    return true;
}

std::vector<SDRMFormat> Aquamarine::CTabOutput::getRenderFormats() {
    if (auto be = backend.lock())
        return be->getRenderFormats();
    return {};
}

Aquamarine::CTabBackend::~CTabBackend() {
    if (m_pClient)
        tab_client_disconnect(m_pClient);
}

Aquamarine::CTabBackend::CTabBackend(SP<CBackend> backend_) : backend(backend_) {

    timerFd  = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    struct itimerspec its = {
        .it_interval = {0, 1000000},  // continuous
        .it_value    = {0, 1000000},
    };

    timerfd_settime(timerFd, 0, &its, NULL);
}

eBackendType Aquamarine::CTabBackend::type() {
    return eBackendType::AQ_BACKEND_TAB;
}

bool Aquamarine::CTabBackend::start() {

    if(!m_pClient) m_pClient = tab_client_connect_default(std::getenv("SHIFT_SESSION_TOKEN"));
    if (!m_pClient) {
        std::cout << "tab backend: Client failed to connect\n";
        return false;
    }

    std::cout << "tab backend: Client connected successfully.\n";

    for (size_t i = 0; i < tab_client_get_monitor_count(m_pClient); ++i) {
        char* mon_id = tab_client_get_monitor_id(m_pClient, i);
        auto monitor_info = tab_client_get_monitor_info(m_pClient, mon_id);
        createOutput(&monitor_info);
        tab_client_free_monitor_info(&monitor_info);
        tab_client_string_free(mon_id);
    }

    backend.lock()->events.pollFDsChanged.emit();

    return true;
}

std::vector<SP<SPollFD>> Aquamarine::CTabBackend::pollFDs() {
    if (!m_pClient)
        return {};

    return {SP<SPollFD>(new SPollFD{.fd=timerFd, .onSignal=[this]() { dispatchEvents(); }})};
}

int Aquamarine::CTabBackend::drmFD() {
    return tab_client_drm_fd(m_pClient);
}

int Aquamarine::CTabBackend::drmRenderNodeFD() {
    return tab_client_drm_fd(m_pClient);
}

bool Aquamarine::CTabBackend::dispatchEvents() {
    uint64_t expirations;
    read(timerFd, &expirations, sizeof(expirations));
    if (!m_pClient)
        return true;
    tab_client_poll_events(m_pClient);
    bool pointerDirty = false, touchDirty = false;
    TabEvent event;
    while (tab_client_next_event(m_pClient, &event)) {
        switch (event.event_type) {
            case TAB_EVENT_FRAME_DONE: {
                auto mon_id = event.data.frame_done;
                for (auto& output : outputs) {
                    if (output->monitor_id == mon_id) {
                        output->needsFrame = false;
                        output->events.present.emit(IOutput::SPresentEvent{.presented = true});
                        break;
                    }
                }
                tab_client_string_free(mon_id);
                break;
            }
            case TabEventType::TAB_EVENT_MONITOR_ADDED: {
                auto mon = event.data.monitor_added;
                backend.lock()->log(AQ_LOG_TRACE, std::format("tab backend: Monitor {} added", mon.id));
                createOutput(&mon);
                tab_client_free_monitor_info(&mon);
                break;
            }
            case TabEventType::TAB_EVENT_MONITOR_REMOVED: {
                auto mon_id = event.data.monitor_removed;
                backend.lock()->log(AQ_LOG_TRACE, std::format("tab backend: Monitor {} removed", mon_id));
                // find and remove the output
                std::erase_if(outputs, [&](const auto& output) {
                    if (output->name == mon_id) {
                        output->destroy();
                        return true;
                    }
                    return false;
                });
                tab_client_string_free(mon_id);
                break;
            }
            case TabEventType::TAB_EVENT_INPUT: {
                handleInput(&event.data.input, pointerDirty, touchDirty);
                break;
            }
            default:
                backend.lock()->log(AQ_LOG_DEBUG, std::format("tab backend: Got an unhandled event of type {}", (int)event.event_type));
                break;
        }
    }

    if (pointerDirty && m_pPointer){
        m_pPointer->events.frame.emit();
    }
    if (touchDirty && m_pTouch)
        m_pTouch->events.frame.emit();
    TabFrameTarget target;
    for(auto& output : outputs)
        if (tab_client_acquire_frame(m_pClient, output->monitor_id.c_str(), &target) == TAB_ACQUIRE_OK)
            {
                output->needsFrame = true;
                output->events.frame.emit();
            }
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



void Aquamarine::CTabBackend::handleInput(TabInputEvent* event,
    bool& pointerDirty,
    bool& touchDirty
) {

    switch (event->kind) {
        case TAB_INPUT_KIND_KEY: {
            if (!m_pKeyboard) {
                m_pKeyboard = SP<CTabKeyboard>(new CTabKeyboard());
                backend.lock()->events.newKeyboard.emit(m_pKeyboard);
            }
            auto& key = event->data.key;
            m_pKeyboard->events.key.emit(IKeyboard::SKeyEvent{
                .timeMs  = (uint32_t)(key.time_usec / 1000),
                .key     = key.key,
                .pressed = key.state == TabKeyState::TAB_KEY_PRESSED,
            });
            break;
        }
        case TAB_INPUT_KIND_POINTER_BUTTON: {
            if (!m_pPointer) {
                m_pPointer = SP<CTabPointer>(new CTabPointer());
                backend.lock()->events.newPointer.emit(m_pPointer);
            }
            auto& button = event->data.pointer_button;
            m_pPointer->events.button.emit(IPointer::SButtonEvent{
                .timeMs  = (uint32_t)(button.time_usec / 1000),
                .button  = button.button,
                .pressed = button.state == TabButtonState::TAB_BUTTON_PRESSED,
            });
            pointerDirty = true;
            break;
        }
        case TAB_INPUT_KIND_POINTER_MOTION: {
            if (!m_pPointer) {
                m_pPointer = SP<CTabPointer>(new CTabPointer());
                backend.lock()->events.newPointer.emit(m_pPointer);
            }
            auto& motion = event->data.pointer_motion;
            auto event = IPointer::SMoveEvent{
                .timeMs = (uint32_t)(motion.time_usec / 1000),
                .delta  = {motion.dx, motion.dy},
                .unaccel  = {motion.unaccel_dx, motion.unaccel_dy},
                

            };
            m_pPointer->events.move.emit(event);
            // debug print
            
            pointerDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TOUCH_DOWN: {
            if (!m_pTouch) {
                m_pTouch = SP<CTabTouch>(new CTabTouch());
                backend.lock()->events.newTouch.emit(m_pTouch);
            }
            auto& touch = event->data.touch_down;
            m_pTouch->events.down.emit(ITouch::SDownEvent{
                .timeMs  = (uint32_t)(touch.time_usec / 1000),
                .touchID = touch.contact.id,
                .pos     = {touch.contact.x, touch.contact.y},
            });
            touchDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TOUCH_UP: {
            if (!m_pTouch) {
                m_pTouch = SP<CTabTouch>(new CTabTouch());
                backend.lock()->events.newTouch.emit(m_pTouch);
            }
            auto& touch = event->data.touch_up;
            m_pTouch->events.up.emit(ITouch::SUpEvent{
                .timeMs  = (uint32_t)(touch.time_usec / 1000),
                .touchID = touch.contact_id,
            });
            touchDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TOUCH_MOTION: {
            if (!m_pTouch) {
                m_pTouch = SP<CTabTouch>(new CTabTouch());
                backend.lock()->events.newTouch.emit(m_pTouch);
            }
            auto& touch = event->data.touch_motion;
            m_pTouch->events.move.emit(ITouch::SMotionEvent{
                .timeMs  = (uint32_t)(touch.time_usec / 1000),
                .touchID = touch.contact.id,
                .pos     = {touch.contact.x, touch.contact.y},
            });
            touchDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TABLET_TOOL_AXIS: {
            if (!m_pTablet) {
                m_pTablet = SP<CTabTablet>(new CTabTablet());
                backend.lock()->events.newTablet.emit(m_pTablet);
            }
            auto& axis = event->data.tablet_tool_axis;
            auto  tool = SP<CTabTabletTool>(new CTabTabletTool());
            m_pTablet->events.axis.emit(ITablet::SAxisEvent{
                .tool   = tool,
                .timeMs = (uint32_t)(axis.time_usec / 1000),
                .absolute = {axis.axes.x, axis.axes.y},
                .tilt = {axis.axes.tilt_x, axis.axes.tilt_y},
                .pressure = axis.axes.pressure,
                .distance = axis.axes.distance,
                .rotation = axis.axes.rotation,
            });
            
            break;
        }
        case TAB_INPUT_KIND_TABLET_TOOL_PROXIMITY: {
            if (!m_pTablet) {
                m_pTablet = SP<CTabTablet>(new CTabTablet());
                backend.lock()->events.newTablet.emit(m_pTablet);
            }
            auto& proximity = event->data.tablet_tool_proximity;
            auto  tool      = SP<CTabTabletTool>(new CTabTabletTool());
            m_pTablet->events.proximity.emit(ITablet::SProximityEvent{
                .tool   = tool,
                .timeMs = (uint32_t)(proximity.time_usec / 1000),
                .in     = proximity.in_proximity,
            });
            break;
        }
        case TAB_INPUT_KIND_TABLET_TOOL_TIP: {
            if (!m_pTablet) {
                m_pTablet = SP<CTabTablet>(new CTabTablet());
                backend.lock()->events.newTablet.emit(m_pTablet);
            }
            auto& tip  = event->data.tablet_tool_tip;
            auto  tool = SP<CTabTabletTool>(new CTabTabletTool());
            m_pTablet->events.tip.emit(ITablet::STipEvent{
                .tool   = tool,
                .timeMs = (uint32_t)(tip.time_usec / 1000),
                .down   = tip.state == TAB_TIP_DOWN,
            });
            
            break;
        }
        case TAB_INPUT_KIND_TABLET_TOOL_BUTTON: {
            if (!m_pTablet) {
                m_pTablet = SP<CTabTablet>(new CTabTablet());
                backend.lock()->events.newTablet.emit(m_pTablet);
            }
            auto& button = event->data.tablet_tool_button;
            auto  tool   = SP<CTabTabletTool>(new CTabTabletTool());
            m_pTablet->events.button.emit(ITablet::SButtonEvent{
                .tool   = tool,
                .timeMs = (uint32_t)(button.time_usec / 1000),
                .button = button.button,
                .down   = button.state == TAB_BUTTON_PRESSED,
            });
            
            break;
        }
        case TAB_INPUT_KIND_SWITCH_TOGGLE: {
            if (!m_pSwitch) {
                m_pSwitch = SP<CTabSwitch>(new CTabSwitch());
                backend.lock()->events.newSwitch.emit(m_pSwitch);
            }
            auto& sw = event->data.switch_toggle;
            m_pSwitch->events.fire.emit(ISwitch::SFireEvent{
                .timeMs = (uint32_t)(sw.time_usec / 1000),
                .type = sw.switch_type == TAB_SWITCH_LID ? ISwitch::eSwitchType::AQ_SWITCH_TYPE_LID : ISwitch::eSwitchType::AQ_SWITCH_TYPE_TABLET_MODE,
                .enable = sw.state == TAB_SWITCH_ON,
            });
            break;
        }
        case TAB_INPUT_KIND_POINTER_MOTION_ABSOLUTE: {
            if (!m_pPointer) {
                m_pPointer = SP<CTabPointer>(new CTabPointer());
                backend.lock()->events.newPointer.emit(m_pPointer);
            }

            auto& abs = event->data.pointer_motion_absolute;
            m_pPointer->events.warp.emit(IPointer::SWarpEvent{
                .timeMs   = (uint32_t)(abs.time_usec / 1000),
                .absolute = {abs.x, abs.y},
            });

            pointerDirty = true;
            break;
        }
        case TAB_INPUT_KIND_POINTER_AXIS: {
            if (!m_pPointer) {
                m_pPointer = SP<CTabPointer>(new CTabPointer());
                backend.lock()->events.newPointer.emit(m_pPointer);
            }

            auto& axis = event->data.pointer_axis;

            IPointer::SAxisEvent ev;
            ev.timeMs = (uint32_t)(axis.time_usec / 1000);

            ev.axis = axis.orientation == TAB_AXIS_VERTICAL ?
                IPointer::AQ_POINTER_AXIS_VERTICAL :
                IPointer::AQ_POINTER_AXIS_HORIZONTAL;

            switch (axis.source) {
                case TAB_AXIS_SOURCE_WHEEL:
                    ev.source = IPointer::AQ_POINTER_AXIS_SOURCE_WHEEL;
                    break;
                case TAB_AXIS_SOURCE_FINGER:
                    ev.source = IPointer::AQ_POINTER_AXIS_SOURCE_FINGER;
                    break;
                case TAB_AXIS_SOURCE_CONTINUOUS:
                    ev.source = IPointer::AQ_POINTER_AXIS_SOURCE_CONTINUOUS;
                    break;
                case TAB_AXIS_SOURCE_WHEEL_TILT:
                    ev.source = IPointer::AQ_POINTER_AXIS_SOURCE_TILT;
                    break;
            }

            ev.delta    = axis.delta;
            ev.discrete = axis.delta_discrete;

            m_pPointer->events.axis.emit(ev);
            pointerDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TOUCH_FRAME: {
            if (m_pTouch)
                m_pTouch->events.frame.emit();

            touchDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TOUCH_CANCEL: {
            if (!m_pTouch) {
                m_pTouch = SP<CTabTouch>(new CTabTouch());
                backend.lock()->events.newTouch.emit(m_pTouch);
            }

            auto& cancel = event->data.touch_cancel;
            m_pTouch->events.cancel.emit(ITouch::SCancelEvent{
                .timeMs  = (uint32_t)(cancel.time_usec / 1000),
                .touchID = -1, // cancel all
            });

            touchDirty = true;
            break;
        }
        case TAB_INPUT_KIND_TABLET_PAD_BUTTON: {
            if (!m_pTabletPad) {
                m_pTabletPad = SP<CTabTabletPad>(new CTabTabletPad());
                backend.lock()->events.newTabletPad.emit(m_pTabletPad);
            }

            auto& b = event->data.tablet_pad_button;
            m_pTabletPad->events.button.emit(ITabletPad::SButtonEvent{
                .timeMs = (uint32_t)(b.time_usec / 1000),
                .button = b.button,
                .down   = b.state == TAB_BUTTON_PRESSED,
                .mode   = 0,
                .group  = 0,
            });
            break;
        }
        case TAB_INPUT_KIND_TABLET_PAD_RING: {
            if (!m_pTabletPad) {
                m_pTabletPad = SP<CTabTabletPad>(new CTabTabletPad());
                backend.lock()->events.newTabletPad.emit(m_pTabletPad);
            }

            auto& r = event->data.tablet_pad_ring;
            m_pTabletPad->events.ring.emit(ITabletPad::SRingEvent{
                .timeMs = (uint32_t)(r.time_usec / 1000),
                .source = ITabletPad::AQ_TABLET_PAD_RING_SOURCE_FINGER,
                .ring   = (uint16_t)r.ring,
                .pos    = r.position,
                .mode   = 0,
            });
            break;
        }
        case TAB_INPUT_KIND_TABLET_PAD_STRIP: {
            if (!m_pTabletPad) {
                m_pTabletPad = SP<CTabTabletPad>(new CTabTabletPad());
                backend.lock()->events.newTabletPad.emit(m_pTabletPad);
            }

            auto& s = event->data.tablet_pad_strip;
            m_pTabletPad->events.strip.emit(ITabletPad::SStripEvent{
                .timeMs = (uint32_t)(s.time_usec / 1000),
                .source = ITabletPad::AQ_TABLET_PAD_STRIP_SOURCE_FINGER,
                .strip  = (uint16_t)s.strip,
                .pos    = s.position,
                .mode   = 0,
            });
            break;
        }
        case TAB_INPUT_KIND_GESTURE_SWIPE_BEGIN: {
    if (!m_pPointer) {
        m_pPointer = SP<CTabPointer>(new CTabPointer());
        backend.lock()->events.newPointer.emit(m_pPointer);
    }

    auto& g = event->data.swipe_begin;
    m_pPointer->events.swipeBegin.emit(IPointer::SSwipeBeginEvent{
        .timeMs  = (uint32_t)(g.time_usec / 1000),
        .fingers = g.fingers,
    });
    break;
}

case TAB_INPUT_KIND_GESTURE_SWIPE_UPDATE: {
    if (!m_pPointer) {
        m_pPointer = SP<CTabPointer>(new CTabPointer());
        backend.lock()->events.newPointer.emit(m_pPointer);
    }

    auto& g = event->data.swipe_update;
    m_pPointer->events.swipeUpdate.emit(IPointer::SSwipeUpdateEvent{
        .timeMs  = (uint32_t)(g.time_usec / 1000),
        .fingers = g.fingers,
        .delta   = { g.dx, g.dy },
    });
    break;
}

case TAB_INPUT_KIND_GESTURE_SWIPE_END: {
    if (!m_pPointer) {
        m_pPointer = SP<CTabPointer>(new CTabPointer());
        backend.lock()->events.newPointer.emit(m_pPointer);
    }

    auto& g = event->data.swipe_end;
    m_pPointer->events.swipeEnd.emit(IPointer::SSwipeEndEvent{
        .timeMs    = (uint32_t)(g.time_usec / 1000),
        .cancelled = g.cancelled,
    });
    break;
}
case TAB_INPUT_KIND_GESTURE_PINCH_BEGIN: {
    if (!m_pPointer) {
        m_pPointer = SP<CTabPointer>(new CTabPointer());
        backend.lock()->events.newPointer.emit(m_pPointer);
    }

    auto& g = event->data.pinch_begin;
    m_pPointer->events.pinchBegin.emit(IPointer::SPinchBeginEvent{
        .timeMs  = (uint32_t)(g.time_usec / 1000),
        .fingers = g.fingers,
    });
    break;
}

case TAB_INPUT_KIND_GESTURE_PINCH_UPDATE: {
    if (!m_pPointer) {
        m_pPointer = SP<CTabPointer>(new CTabPointer());
        backend.lock()->events.newPointer.emit(m_pPointer);
    }

    auto& g = event->data.pinch_update;
    m_pPointer->events.pinchUpdate.emit(IPointer::SPinchUpdateEvent{
        .timeMs   = (uint32_t)(g.time_usec / 1000),
        .fingers  = g.fingers,
        .delta    = { g.dx, g.dy },
        .scale    = g.scale,
        .rotation = g.rotation,
    });
    break;
}

case TAB_INPUT_KIND_GESTURE_PINCH_END: {
    if (!m_pPointer) {
        m_pPointer = SP<CTabPointer>(new CTabPointer());
        backend.lock()->events.newPointer.emit(m_pPointer);
    }

    auto& g = event->data.pinch_end;
    m_pPointer->events.pinchEnd.emit(IPointer::SPinchEndEvent{
        .timeMs    = (uint32_t)(g.time_usec / 1000),
        .cancelled = g.cancelled,
    });
    break;
}
case TAB_INPUT_KIND_GESTURE_HOLD_BEGIN: {
    if (!m_pPointer) {
        m_pPointer = SP<CTabPointer>(new CTabPointer());
        backend.lock()->events.newPointer.emit(m_pPointer);
    }

    auto& g = event->data.hold_begin;
    m_pPointer->events.holdBegin.emit(IPointer::SHoldBeginEvent{
        .timeMs  = (uint32_t)(g.time_usec / 1000),
        .fingers = g.fingers,
    });
    break;
}

case TAB_INPUT_KIND_GESTURE_HOLD_END: {
    if (!m_pPointer) {
        m_pPointer = SP<CTabPointer>(new CTabPointer());
        backend.lock()->events.newPointer.emit(m_pPointer);
    }

    auto& g = event->data.hold_end;
    m_pPointer->events.holdEnd.emit(IPointer::SHoldEndEvent{
        .timeMs    = (uint32_t)(g.time_usec / 1000),
        .cancelled = g.cancelled,
    });
    break;
}

        default: backend.lock()->log(AQ_LOG_DEBUG, std::format("tab backend: Got an unhandled input event of type {}", (int)event->kind)); break;
    }
}

std::vector<SDRMFormat> Aquamarine::CTabBackend::getRenderFormats() {
    for (const auto& impl : backend->getImplementations()) {
        if (impl->type() != AQ_BACKEND_DRM || impl->getRenderableFormats().empty())
            continue;
        return impl->getRenderableFormats();
    }

    // formats probably supported by EGL
    return {SDRMFormat{.drmFormat = DRM_FORMAT_XRGB8888, .modifiers = {DRM_FORMAT_INVALID}},
            SDRMFormat{.drmFormat = DRM_FORMAT_XBGR8888, .modifiers = {DRM_FORMAT_INVALID}},
            SDRMFormat{.drmFormat = DRM_FORMAT_RGBX8888, .modifiers = {DRM_FORMAT_INVALID}},
            SDRMFormat{.drmFormat = DRM_FORMAT_BGRX8888, .modifiers = {DRM_FORMAT_INVALID}},
            SDRMFormat{.drmFormat = DRM_FORMAT_ARGB8888, .modifiers = {DRM_FORMAT_INVALID}},
            SDRMFormat{.drmFormat = DRM_FORMAT_ABGR8888, .modifiers = {DRM_FORMAT_INVALID}},
            SDRMFormat{.drmFormat = DRM_FORMAT_RGBA8888, .modifiers = {DRM_FORMAT_INVALID}},
            SDRMFormat{.drmFormat = DRM_FORMAT_BGRA8888, .modifiers = {DRM_FORMAT_INVALID}},
            SDRMFormat{.drmFormat = DRM_FORMAT_XRGB2101010, .modifiers = {DRM_FORMAT_MOD_LINEAR}},
            SDRMFormat{.drmFormat = DRM_FORMAT_XBGR2101010, .modifiers = {DRM_FORMAT_MOD_LINEAR}},
            SDRMFormat{.drmFormat = DRM_FORMAT_RGBX1010102, .modifiers = {DRM_FORMAT_MOD_LINEAR}},
            SDRMFormat{.drmFormat = DRM_FORMAT_BGRX1010102, .modifiers = {DRM_FORMAT_MOD_LINEAR}},
            SDRMFormat{.drmFormat = DRM_FORMAT_ARGB2101010, .modifiers = {DRM_FORMAT_MOD_LINEAR}},
            SDRMFormat{.drmFormat = DRM_FORMAT_ABGR2101010, .modifiers = {DRM_FORMAT_MOD_LINEAR}},
            SDRMFormat{.drmFormat = DRM_FORMAT_RGBA1010102, .modifiers = {DRM_FORMAT_MOD_LINEAR}},
            SDRMFormat{.drmFormat = DRM_FORMAT_BGRA1010102, .modifiers = {DRM_FORMAT_MOD_LINEAR}}};
}

std::vector<SDRMFormat> Aquamarine::CTabBackend::getCursorFormats() {
    return {};
}

bool Aquamarine::CTabBackend::createOutput(const TabMonitorInfo* monitor_info) {
    auto output = SP<CTabOutput>(new CTabOutput(*monitor_info, self));
    output->self = output;
    outputs.emplace_back(output);

    backend.lock()->events.newOutput.emit(output);

    return true;
}

SP<IAllocator> Aquamarine::CTabBackend::preferredAllocator() {
    return nullptr;
}

std::vector<SP<IAllocator>> Aquamarine::CTabBackend::getAllocators() {
    return {};
}

CWeakPointer<IBackendImplementation> Aquamarine::CTabBackend::getPrimary() {
    return self;
}