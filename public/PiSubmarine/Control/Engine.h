#pragma once

#include <mutex>
#include <optional>
#include <memory>

#include "PiSubmarine/Control/Api/Input/ISink.h"
#include "PiSubmarine/Control/Api/Input/OperatorCommand.h"
#include "EngineErrorCode.h"
#include "PiSubmarine/Logging/Api/IFactory.h"
#include "PiSubmarine/Control/Pilot/Api/IController.h"
#include "PiSubmarine/Lease/Api/ILeaseValidator.h"
#include "PiSubmarine/Lease/Api/IResourceRegistry.h"
#include "PiSubmarine/Time/ITickable.h"

namespace PiSubmarine::Control
{
    class Engine final : public Api::Input::ISink, public Time::ITickable
    {
    public:
        // Thread-safe for Submit(...). Tick(...) must run on the single deterministic control thread.
        Engine(
            Lease::Api::IResourceRegistry& resourceRegistry,
            const Lease::Api::ILeaseValidator& leaseValidator,
            Pilot::Api::IController& manualController,
            Pilot::Api::IController& holdPositionController,
            Logging::Api::IFactory& loggerFactory);

        [[nodiscard]] Error::Api::Result<void> Submit(const Api::Input::OperatorCommand& command) override;
        void Tick(const std::chrono::nanoseconds& uptime, const std::chrono::nanoseconds& deltaTime) override;

    private:
        enum class PilotMode
        {
            Manual,
            HoldPosition
        };

        void ApplyCommand(const Api::Input::OperatorCommand& command);
        void UpdateActiveController();
        [[nodiscard]] Pilot::Api::IController& GetController(PilotMode mode) noexcept;
        void LogIfError(const Error::Api::Result<void>& result, const char* action) const;

        Lease::Api::IResourceRegistry& m_ResourceRegistry;
        const Lease::Api::ILeaseValidator& m_LeaseValidator;
        Pilot::Api::IController& m_ManualController;
        Pilot::Api::IController& m_HoldPositionController;
        std::shared_ptr<spdlog::logger> m_Logger;

        mutable std::mutex m_Mutex;
        std::optional<Api::Input::OperatorCommand> m_PendingCommand;

        Pilot::Api::Input m_Input;
        PilotMode m_DesiredMode = PilotMode::Manual;
        PilotMode m_ActiveMode = PilotMode::Manual;
        bool m_IsInitialized = false;
    };
}
