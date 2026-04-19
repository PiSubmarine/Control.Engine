#pragma once

#include <mutex>
#include <optional>

#include "PiSubmarine/Control/Api/Input/OperatorCommand.h"
#include "PiSubmarine/Control/Engine/ErrorCode.h"
#include "PiSubmarine/Control/Pilot/Api/IController.h"
#include "PiSubmarine/Lease/Api/ILeaseValidator.h"
#include "PiSubmarine/Lease/Api/IResourceRegistry.h"
#include "PiSubmarine/Lease/Api/Identifiers.h"
#include "PiSubmarine/Time/ITickable.h"

namespace PiSubmarine::Control
{
    class Engine final : public Time::ITickable
    {
    public:
        // Thread-safe for Submit(...). Tick(...) must run on the single deterministic control thread.
        Engine(
            Lease::Api::IResourceRegistry& resourceRegistry,
            const Lease::Api::ILeaseValidator& leaseValidator,
            Pilot::Api::IController& manualController,
            Pilot::Api::IController& holdPositionController);

        [[nodiscard]] Error::Api::Result<void> Submit(
            const Lease::Api::LeaseId& leaseId,
            const Api::Input::OperatorCommand& command);
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
        static void ThrowIfError(const Error::Api::Result<void>& result, const char* action);

        Lease::Api::IResourceRegistry& m_ResourceRegistry;
        const Lease::Api::ILeaseValidator& m_LeaseValidator;
        Pilot::Api::IController& m_ManualController;
        Pilot::Api::IController& m_HoldPositionController;

        mutable std::mutex m_Mutex;
        std::optional<Api::Input::OperatorCommand> m_PendingCommand;

        Pilot::Api::Input m_Input;
        PilotMode m_DesiredMode = PilotMode::Manual;
        PilotMode m_ActiveMode = PilotMode::Manual;
        bool m_IsInitialized = false;
    };
}
