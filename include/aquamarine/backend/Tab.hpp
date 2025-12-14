#pragma once

#include "./Backend.hpp"
#include "../allocator/Swapchain.hpp"
#include "../output/Output.hpp"
#include <hyprutils/memory/WeakPtr.hpp>

namespace Aquamarine {
    class CBackend;
    class CTabBackend;
    class IAllocator;

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
        CTabOutput(const std::string& name_, Hyprutils::Memory::CWeakPointer<CTabBackend> backend_);

        Hyprutils::Memory::CWeakPointer<CTabBackend> backend;

        Hyprutils::Memory::CSharedPointer<std::function<void()>> framecb;
        bool                                                     frameScheduled = false;

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
        virtual bool                                                       createOutput(const std::string& name = "");
        virtual Hyprutils::Memory::CSharedPointer<IAllocator>              preferredAllocator();
        virtual std::vector<Hyprutils::Memory::CSharedPointer<IAllocator>> getAllocators();
        virtual Hyprutils::Memory::CWeakPointer<IBackendImplementation>    getPrimary();

        Hyprutils::Memory::CWeakPointer<CTabBackend>                       self;
        virtual int                                                        drmRenderNodeFD();

      private:
        CTabBackend(Hyprutils::Memory::CSharedPointer<CBackend> backend_);

        Hyprutils::Memory::CWeakPointer<CBackend>                      backend;
        std::vector<Hyprutils::Memory::CSharedPointer<CTabOutput>>     outputs;

        size_t                                                         outputIDCounter = 0;

        friend class CBackend;
        friend class CTabOutput;
    };
};