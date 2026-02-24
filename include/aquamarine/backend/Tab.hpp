#pragma once

#include "./Backend.hpp"
#include "../allocator/Swapchain.hpp"
#include "../output/Output.hpp"
#include <hyprutils/memory/WeakPtr.hpp>
#include <tab_client.h>
#include <optional>
#include <vector>
#include <ctime>
#include <string>
#include <string_view>

namespace Aquamarine {
    class CBackend;
    class CTabBackend;
    class IAllocator;
    class IKeyboard;
    class IPointer;
    class ITouch;
    class ITablet;
    class ITabletPad;
    class ISwitch;

    class CTabOutput : public IOutput {
      public:
        ~CTabOutput() override;

        bool                                                      commit() override;
        bool                                                      test() override;
        Hyprutils::Memory::CSharedPointer<IBackendImplementation> getBackend() override;
        void                                                      scheduleFrame(const scheduleFrameReason reason = AQ_SCHEDULE_UNKNOWN) override;
        bool                                                      destroy() override;
        std::vector<SDRMFormat>                                   getRenderFormats() override;
        std::string tracyPlotName;
        Hyprutils::Memory::CWeakPointer<CTabOutput> self;

      private:
        CTabOutput(const TabMonitorInfo& monitor_info, Hyprutils::Memory::CWeakPointer<CTabBackend> backend_);

        Hyprutils::Memory::CWeakPointer<CTabBackend> backend;
        std::string                                  monitorID;
        int                                          refreshRateHz     = 60;
        int                                          refreshIntervalNs = 0;
        timespec                                     lastPresentTime {};
        uint32_t                                     presentSeq = 0;
        bool                                         frameEventScheduled = false;
        Hyprutils::Memory::CSharedPointer<std::function<void(void)>> frameIdle;

        friend class CTabBackend;
    };

    class CTabBackend : public IBackendImplementation {
      public:
        explicit CTabBackend(Hyprutils::Memory::CSharedPointer<CBackend> backend_);
        ~CTabBackend() override;

        eBackendType                                               type() override;
        bool                                                       start() override;
        std::vector<Hyprutils::Memory::CSharedPointer<SPollFD>>    pollFDs() override;
        int                                                        drmFD() override;
        int                                                        drmRenderNodeFD() override;
        bool                                                       dispatchEvents() override;
        uint32_t                                                   capabilities() override;
        bool                                                       setCursor(Hyprutils::Memory::CSharedPointer<IBuffer> buffer, const Hyprutils::Math::Vector2D& hotspot);
        void                                                       onReady() override;
        std::vector<SDRMFormat>                                    getRenderFormats() override;
        std::vector<SDRMFormat>                                    getCursorFormats() override;
        bool                                                       createOutput(const TabMonitorInfo& monitor_info);
        Hyprutils::Memory::CSharedPointer<IAllocator>              preferredAllocator() override;
        std::vector<Hyprutils::Memory::CSharedPointer<IAllocator>> getAllocators() override;
        Hyprutils::Memory::CWeakPointer<IBackendImplementation>    getPrimary() override;

        void handleInput(TabInputEvent* event, bool& pointerDirty, bool& touchDirty);

        Hyprutils::Memory::CWeakPointer<CTabBackend> self;

      private:
        struct SPendingRelease {
            std::string monitorID;
            uint32_t    bufferIndex = 0;
            int         releaseFenceFD = -1;
        };

        Hyprutils::Memory::CWeakPointer<CBackend>                  backend;
        std::vector<Hyprutils::Memory::CSharedPointer<CTabOutput>> outputs;
        std::vector<SPendingRelease>                               pendingReleases;
        TabClientHandle*                                           client = nullptr;

        Hyprutils::Memory::CSharedPointer<IKeyboard>   keyboard;
        Hyprutils::Memory::CSharedPointer<IPointer>    pointer;
        Hyprutils::Memory::CSharedPointer<ITouch>      touch;
        Hyprutils::Memory::CSharedPointer<ITablet>     tablet;
        Hyprutils::Memory::CSharedPointer<ITabletPad>  tabletPad;
        Hyprutils::Memory::CSharedPointer<ISwitch>     switchDev;

        TabClientHandle* ensureClient();
        CTabOutput*      findOutputByID(std::string_view id);
        bool             isFenceSignaled(int fd) const;
        void             queuePendingRelease(SPendingRelease&& pending);
        void             flushPendingReleases();

        friend class CTabOutput;
    };
};
