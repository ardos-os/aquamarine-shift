#pragma once

#include "./Backend.hpp"
#include "../allocator/Swapchain.hpp"
#include "../output/Output.hpp"
#include <hyprutils/memory/WeakPtr.hpp>
#include <memory>
#include <tab_client.h>

namespace Aquamarine {
    class CBackend;
    class CTabBackend;
    class IAllocator;
    class IKeyboard;
    class IPointer;
    class ITouch;
    class ITablet;
    class ISwitch;

    class CTabOutput : public IOutput {
      public:
        virtual ~CTabOutput();
        virtual bool                                                      commit();
        virtual bool                                                      test();
        virtual Hyprutils::Memory::CSharedPointer<IBackendImplementation> getBackend();
        virtual void                                                      scheduleFrame(const scheduleFrameReason reason = AQ_SCHEDULE_UNKNOWN);
        virtual bool                                                      destroy();
        virtual std::vector<SDRMFormat>                                   getRenderFormats();
        Hyprutils::Memory::CWeakPointer<CTabOutput>                       self;

      private:
        CTabOutput(const TabMonitorInfo& monitor_info, Hyprutils::Memory::CWeakPointer<CTabBackend> backend);

        Hyprutils::Memory::CWeakPointer<CTabBackend> backend;

        std::string monitor_id;

        friend class CTabBackend;
    };

    class CTabBackend : public IBackendImplementation {
      public:
        virtual ~CTabBackend();
        virtual eBackendType                                               type();
        virtual bool                                                       start();
        virtual std::vector<Hyprutils::Memory::CSharedPointer<SPollFD>>    pollFDs();
        virtual int                                                        drmFD();
        virtual bool                                                       dispatchEvents();
        virtual uint32_t                                                   capabilities();
        virtual bool                                                       setCursor(Hyprutils::Memory::CSharedPointer<IBuffer> buffer, const Hyprutils::Math::Vector2D& hotspot);
        virtual void                                                       onReady();
        virtual std::vector<SDRMFormat>                                    getRenderFormats();
        virtual std::vector<SDRMFormat>                                    getCursorFormats();
        bool                                                       createOutput(const TabMonitorInfo* monitor_info);
        virtual Hyprutils::Memory::CSharedPointer<IAllocator>              preferredAllocator();
        virtual std::vector<Hyprutils::Memory::CSharedPointer<IAllocator>> getAllocators();
        virtual Hyprutils::Memory::CWeakPointer<IBackendImplementation>    getPrimary();

        Hyprutils::Memory::CWeakPointer<CTabBackend>                       self;
        virtual int                                                        drmRenderNodeFD();
        
      private:
        int timerFd;
        CTabBackend(Hyprutils::Memory::CSharedPointer<CBackend> backend_);

        void handleInput(TabInputEvent* event,bool& pointerDirty,bool& touchDirty);
        Hyprutils::Memory::CWeakPointer<CBackend>                      backend;
        std::vector<Hyprutils::Memory::CSharedPointer<CTabOutput>>     outputs;
        TabClientHandle*                                               m_pClient = nullptr;

        Hyprutils::Memory::CSharedPointer<IKeyboard>                   m_pKeyboard;
        Hyprutils::Memory::CSharedPointer<IPointer>                    m_pPointer;
        Hyprutils::Memory::CSharedPointer<ITouch>                      m_pTouch;
        Hyprutils::Memory::CSharedPointer<ITablet>                     m_pTablet;
        Hyprutils::Memory::CSharedPointer<ITabletPad>                  m_pTabletPad;
        Hyprutils::Memory::CSharedPointer<ISwitch>                     m_pSwitch;

        size_t                                                         outputIDCounter = 0;

        friend class CBackend;
        friend class CTabOutput;
    };
};