#include "PiSubmarine/Control/Engine.h"

#include <string>

#include <spdlog/spdlog.h>

#include "PiSubmarine/Error/Api/ErrorCondition.h"
#include "PiSubmarine/Error/Api/MakeError.h"
#include "PiSubmarine/Error/Api/ToStringView.h"

namespace PiSubmarine::Control
{
    namespace
    {
        [[nodiscard]] Lease::Api::ResourceId MakeControlResourceId()
        {
            return Lease::Api::ResourceId{.Value = "control-main"};
        }

        [[nodiscard]] Lease::Api::ResourceDescriptor MakeControlResource()
        {
            return Lease::Api::ResourceDescriptor{
                .Id = MakeControlResourceId(),
                .Policy = Lease::Api::LeasePolicy{
                    .MaxLeases = 1,
                    .LeaseDuration = std::chrono::milliseconds(3000)}};
        }

        [[nodiscard]] Error::Api::Error MakeContractError(const EngineErrorCode code) noexcept
        {
            return Error::Api::MakeError(Error::Api::ErrorCondition::ContractError, make_error_code(code));
        }

        [[nodiscard]] Error::Api::Error MakeCommunicationError(const EngineErrorCode code) noexcept
        {
            return Error::Api::MakeError(Error::Api::ErrorCondition::CommunicationError, make_error_code(code));
        }

        [[nodiscard]] std::string FormatErrorForLog(const Error::Api::Error& error)
        {
            std::string message(Error::Api::ToStringView(error.Condition));

            if (error.HasCause())
            {
                message += " [";
                message += error.Cause.category().name();
                message += ':';
                message += std::to_string(error.Cause.value());
                message += "] ";
                message += error.Cause.message();
            }

            return message;
        }
    }

    Engine::Engine(
        Lease::Api::IResourceRegistry& resourceRegistry,
        const Lease::Api::ILeaseValidator& leaseValidator,
        Pilot::Api::IController& manualController,
        Pilot::Api::IController& holdPositionController,
        Logging::Api::IFactory& loggerFactory)
        : m_ResourceRegistry(resourceRegistry)
        , m_LeaseValidator(leaseValidator)
        , m_ManualController(manualController)
        , m_HoldPositionController(holdPositionController)
        , m_Logger(loggerFactory.CreateLogger("Control.Engine"))
    {
        LogIfError(m_ResourceRegistry.RegisterResource(MakeControlResource()), "registering control resource");
    }

    Error::Api::Result<void> Engine::Submit(const Api::Input::OperatorCommand& command)
    {
        const auto validation = m_LeaseValidator.ValidateLease(command.LeaseId, MakeControlResourceId());
        if (!validation.has_value())
        {
            return std::unexpected(MakeCommunicationError(EngineErrorCode::LeaseValidationFailed));
        }

        if (!validation->IsValid)
        {
            return std::unexpected(MakeContractError(EngineErrorCode::InvalidControlLease));
        }

        std::scoped_lock lock(m_Mutex);
        m_PendingCommand = command;
        return {};
    }

    void Engine::Tick(const std::chrono::nanoseconds& uptime, const std::chrono::nanoseconds& deltaTime)
    {
        std::optional<Api::Input::OperatorCommand> pendingCommand;
        {
            std::scoped_lock lock(m_Mutex);
            pendingCommand.swap(m_PendingCommand);
        }

        if (pendingCommand.has_value())
        {
            ApplyCommand(*pendingCommand);
        }

        UpdateActiveController();

        auto& activeController = GetController(m_ActiveMode);
        LogIfError(activeController.SetInput(m_Input), "setting pilot input");
        LogIfError(activeController.Step(uptime, deltaTime), "stepping active pilot");
    }

    void Engine::ApplyCommand(const Api::Input::OperatorCommand& command)
    {
        m_Input.Movement = command.Movement;

        if (!command.VerticalControl.Is<Vertical::Api::Command::KeepCurrent>())
        {
            m_Input.VerticalControl = command.VerticalControl;
        }

        if (command.GimbalTarget.has_value())
        {
            m_Input.GimbalTarget = *command.GimbalTarget;
        }

        if (command.LampIntensity.has_value())
        {
            m_Input.LampIntensity = *command.LampIntensity;
        }

        if (command.VideoControl.has_value())
        {
            m_Input.VideoControl = *command.VideoControl;
        }

        if (command.ModeRequest.has_value())
        {
            if (command.ModeRequest->Is<Api::Input::Mode::Manual>())
            {
                m_DesiredMode = PilotMode::Manual;
            }
            else if (command.ModeRequest->Is<Api::Input::Mode::HoldPosition>())
            {
                m_DesiredMode = PilotMode::HoldPosition;
            }
        }
    }

    void Engine::UpdateActiveController()
    {
        if (!m_IsInitialized)
        {
            LogIfError(GetController(PilotMode::HoldPosition).SetActive(false), "deactivating hold-position pilot");
            LogIfError(GetController(PilotMode::Manual).SetActive(false), "deactivating manual pilot");

            const auto activateResult = GetController(m_DesiredMode).SetActive(true);
            LogIfError(activateResult, "activating initial pilot");
            if (!activateResult.has_value())
            {
                return;
            }

            m_ActiveMode = m_DesiredMode;
            m_IsInitialized = true;
            return;
        }

        if (m_ActiveMode == m_DesiredMode)
        {
            return;
        }

        LogIfError(GetController(m_ActiveMode).SetActive(false), "deactivating previous pilot");

        const auto activateResult = GetController(m_DesiredMode).SetActive(true);
        LogIfError(activateResult, "activating selected pilot");
        if (!activateResult.has_value())
        {
            return;
        }

        m_ActiveMode = m_DesiredMode;
    }

    Pilot::Api::IController& Engine::GetController(const PilotMode mode) noexcept
    {
        switch (mode)
        {
            case PilotMode::Manual:
                return m_ManualController;
            case PilotMode::HoldPosition:
                return m_HoldPositionController;
        }

        return m_ManualController;
    }

    void Engine::LogIfError(const Error::Api::Result<void>& result, const char* action) const
    {
        if (!m_Logger || result.has_value())
        {
            return;
        }

        SPDLOG_LOGGER_ERROR(
            m_Logger,
            "Failed while {}: {}",
            action,
            FormatErrorForLog(result.error()));
    }
}
