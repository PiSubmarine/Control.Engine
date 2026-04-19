#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "PiSubmarine/Control/Engine.h"
#include "PiSubmarine/Control/Pilot/Api/IControllerMock.h"
#include "PiSubmarine/Lease/Api/ILeaseValidatorMock.h"
#include "PiSubmarine/Lease/Api/IResourceRegistryMock.h"

namespace PiSubmarine::Control
{
    namespace
    {
        using ::testing::InSequence;
        using ::testing::Return;
        using ::testing::StrictMock;

        [[nodiscard]] Lease::Api::ResourceDescriptor MakeExpectedControlResource()
        {
            return Lease::Api::ResourceDescriptor{
                .Id = Lease::Api::ResourceId{.Value = "control-main"},
                .Policy = Lease::Api::LeasePolicy{
                    .MaxLeases = 1,
                    .LeaseDuration = std::chrono::milliseconds(3000)}};
        }

        [[nodiscard]] Lease::Api::ResourceId MakeExpectedControlResourceId()
        {
            return Lease::Api::ResourceId{.Value = "control-main"};
        }

        [[nodiscard]] Lease::Api::LeaseId MakeValidLeaseId()
        {
            return Lease::Api::LeaseId{.Value = "lease-1"};
        }

        [[nodiscard]] Pilot::Api::Input MakeExpectedInput(
            const Horizontal::Api::Command& movement,
            const Vertical::Api::Command& vertical,
            const Gimbal::Api::Command& gimbal,
            const Lamp::Api::Command& lamp,
            const Video::Api::Command& video)
        {
            return Pilot::Api::Input{
                .Movement = movement,
                .VerticalControl = vertical,
                .GimbalTarget = gimbal,
                .LampIntensity = lamp,
                .VideoControl = video};
        }
    }

    TEST(EngineTest, SubmitAcceptsLatestOperatorCommand)
    {
        StrictMock<Lease::Api::IResourceRegistryMock> resourceRegistry;
        StrictMock<Lease::Api::ILeaseValidatorMock> leaseValidator;
        StrictMock<Pilot::Api::IControllerMock> manualController;
        StrictMock<Pilot::Api::IControllerMock> holdPositionController;
        EXPECT_CALL(resourceRegistry, RegisterResource(MakeExpectedControlResource()))
            .WillOnce(Return(Error::Api::Result<void>{}));
        EXPECT_CALL(leaseValidator, ValidateLease(MakeValidLeaseId(), MakeExpectedControlResourceId()))
            .WillOnce(Return(Error::Api::Result<Lease::Api::LeaseValidation>(Lease::Api::LeaseValidation{.IsValid = true})));
        Engine engine(resourceRegistry, leaseValidator, manualController, holdPositionController);

        EXPECT_TRUE(engine.Submit(MakeValidLeaseId(), Api::Input::OperatorCommand{}).has_value());
    }

    TEST(EngineTest, FirstTickActivatesManualPilotAndStepsItWithDefaultInput)
    {
        StrictMock<Lease::Api::IResourceRegistryMock> resourceRegistry;
        StrictMock<Lease::Api::ILeaseValidatorMock> leaseValidator;
        StrictMock<Pilot::Api::IControllerMock> manualController;
        StrictMock<Pilot::Api::IControllerMock> holdPositionController;
        EXPECT_CALL(resourceRegistry, RegisterResource(MakeExpectedControlResource()))
            .WillOnce(Return(Error::Api::Result<void>{}));
        Engine engine(resourceRegistry, leaseValidator, manualController, holdPositionController);
        const auto expectedInput = Pilot::Api::Input{};
        const auto uptime = std::chrono::nanoseconds(std::chrono::milliseconds(100));
        const auto deltaTime = std::chrono::nanoseconds(std::chrono::milliseconds(10));

        InSequence sequence;
        EXPECT_CALL(holdPositionController, SetActive(false))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetActive(false))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetActive(true))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetInput(expectedInput))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, Step(uptime, deltaTime))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));

        engine.Tick(uptime, deltaTime);
    }

    TEST(EngineTest, TickAppliesSubmittedOperatorCommandToResolvedPilotInput)
    {
        StrictMock<Lease::Api::IResourceRegistryMock> resourceRegistry;
        StrictMock<Lease::Api::ILeaseValidatorMock> leaseValidator;
        StrictMock<Pilot::Api::IControllerMock> manualController;
        StrictMock<Pilot::Api::IControllerMock> holdPositionController;
        EXPECT_CALL(resourceRegistry, RegisterResource(MakeExpectedControlResource()))
            .WillOnce(Return(Error::Api::Result<void>{}));
        EXPECT_CALL(leaseValidator, ValidateLease(MakeValidLeaseId(), MakeExpectedControlResourceId()))
            .WillOnce(Return(Error::Api::Result<Lease::Api::LeaseValidation>(Lease::Api::LeaseValidation{.IsValid = true})));
        Engine engine(resourceRegistry, leaseValidator, manualController, holdPositionController);

        const auto movement = Horizontal::Api::Command::Create(0.5, 0.25).value();
        const auto vertical = Vertical::Api::Command::SetDepthTargetTo(3.0_m);
        const auto gimbal = Gimbal::Api::Command::Create(0.5_rad);
        const auto lamp = Lamp::Api::Command::Create(NormalizedFraction(0.7));
        const auto video = Video::Api::Command::Enable(
            Video::Api::StreamProfile::Standard,
            Video::Api::AutoFocus{});
        const auto expectedInput = MakeExpectedInput(movement, vertical, gimbal, lamp, video);
        const auto uptime = std::chrono::nanoseconds(std::chrono::milliseconds(100));
        const auto deltaTime = std::chrono::nanoseconds(std::chrono::milliseconds(10));

        const Api::Input::OperatorCommand command{
            .Movement = movement,
            .VerticalControl = vertical,
            .GimbalTarget = gimbal,
            .LampIntensity = lamp,
            .VideoControl = video};
        ASSERT_TRUE(engine.Submit(MakeValidLeaseId(), command).has_value());

        InSequence sequence;
        EXPECT_CALL(holdPositionController, SetActive(false))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetActive(false))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetActive(true))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetInput(expectedInput))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, Step(uptime, deltaTime))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));

        engine.Tick(uptime, deltaTime);
    }

    TEST(EngineTest, KeepCurrentVerticalCommandPreservesPreviousVerticalTarget)
    {
        StrictMock<Lease::Api::IResourceRegistryMock> resourceRegistry;
        StrictMock<Lease::Api::ILeaseValidatorMock> leaseValidator;
        StrictMock<Pilot::Api::IControllerMock> manualController;
        StrictMock<Pilot::Api::IControllerMock> holdPositionController;
        EXPECT_CALL(resourceRegistry, RegisterResource(MakeExpectedControlResource()))
            .WillOnce(Return(Error::Api::Result<void>{}));
        EXPECT_CALL(leaseValidator, ValidateLease(MakeValidLeaseId(), MakeExpectedControlResourceId()))
            .WillOnce(Return(Error::Api::Result<Lease::Api::LeaseValidation>(Lease::Api::LeaseValidation{.IsValid = true})));
        EXPECT_CALL(leaseValidator, ValidateLease(MakeValidLeaseId(), MakeExpectedControlResourceId()))
            .WillOnce(Return(Error::Api::Result<Lease::Api::LeaseValidation>(Lease::Api::LeaseValidation{.IsValid = true})));
        Engine engine(resourceRegistry, leaseValidator, manualController, holdPositionController);

        const auto initialMovement = Horizontal::Api::Command::Create(0.5, 0.25).value();
        const auto nextMovement = Horizontal::Api::Command::Create(-0.2, 0.1).value();
        const auto vertical = Vertical::Api::Command::SetDepthTargetTo(3.0_m);
        const auto firstInput = MakeExpectedInput(
            initialMovement,
            vertical,
            Gimbal::Api::Command::Create(0_rad),
            Lamp::Api::Command::Create(NormalizedFraction(0)),
            Video::Api::Command::Disable());
        const auto secondInput = MakeExpectedInput(
            nextMovement,
            vertical,
            Gimbal::Api::Command::Create(0_rad),
            Lamp::Api::Command::Create(NormalizedFraction(0)),
            Video::Api::Command::Disable());
        const auto firstUptime = std::chrono::nanoseconds(std::chrono::milliseconds(100));
        const auto secondUptime = std::chrono::nanoseconds(std::chrono::milliseconds(110));
        const auto deltaTime = std::chrono::nanoseconds(std::chrono::milliseconds(10));

        ASSERT_TRUE(engine.Submit(MakeValidLeaseId(), Api::Input::OperatorCommand{
            .Movement = initialMovement,
            .VerticalControl = vertical}).has_value());

        InSequence sequence;
        EXPECT_CALL(holdPositionController, SetActive(false))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetActive(false))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetActive(true))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetInput(firstInput))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, Step(firstUptime, deltaTime))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetInput(secondInput))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, Step(secondUptime, deltaTime))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));

        engine.Tick(firstUptime, deltaTime);

        ASSERT_TRUE(engine.Submit(MakeValidLeaseId(), Api::Input::OperatorCommand{
            .Movement = nextMovement,
            .VerticalControl = Vertical::Api::Command::KeepCurrentValue()}).has_value());

        engine.Tick(secondUptime, deltaTime);
    }

    TEST(EngineTest, HoldPositionRequestSwitchesToHoldPositionPilot)
    {
        StrictMock<Lease::Api::IResourceRegistryMock> resourceRegistry;
        StrictMock<Lease::Api::ILeaseValidatorMock> leaseValidator;
        StrictMock<Pilot::Api::IControllerMock> manualController;
        StrictMock<Pilot::Api::IControllerMock> holdPositionController;
        EXPECT_CALL(resourceRegistry, RegisterResource(MakeExpectedControlResource()))
            .WillOnce(Return(Error::Api::Result<void>{}));
        EXPECT_CALL(leaseValidator, ValidateLease(MakeValidLeaseId(), MakeExpectedControlResourceId()))
            .WillOnce(Return(Error::Api::Result<Lease::Api::LeaseValidation>(Lease::Api::LeaseValidation{.IsValid = true})));
        Engine engine(resourceRegistry, leaseValidator, manualController, holdPositionController);

        const auto firstUptime = std::chrono::nanoseconds(std::chrono::milliseconds(100));
        const auto secondUptime = std::chrono::nanoseconds(std::chrono::milliseconds(110));
        const auto deltaTime = std::chrono::nanoseconds(std::chrono::milliseconds(10));
        const auto holdInput = MakeExpectedInput(
            Horizontal::Api::Command::Create(0, 0).value(),
            Vertical::Api::Command::KeepCurrentValue(),
            Gimbal::Api::Command::Create(0_rad),
            Lamp::Api::Command::Create(NormalizedFraction(0)),
            Video::Api::Command::Disable());

        InSequence sequence;
        EXPECT_CALL(holdPositionController, SetActive(false))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetActive(false))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetActive(true))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetInput(Pilot::Api::Input{}))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, Step(firstUptime, deltaTime))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(manualController, SetActive(false))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(holdPositionController, SetActive(true))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(holdPositionController, SetInput(holdInput))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));
        EXPECT_CALL(holdPositionController, Step(secondUptime, deltaTime))
            .WillOnce(testing::Return(Error::Api::Result<void>{}));

        engine.Tick(firstUptime, deltaTime);

        ASSERT_TRUE(engine.Submit(MakeValidLeaseId(), Api::Input::OperatorCommand{
            .ModeRequest = Api::Input::Mode::Request::HoldPositionValue()}).has_value());

        engine.Tick(secondUptime, deltaTime);
    }

    TEST(EngineTest, SubmitRejectsInvalidControlLease)
    {
        StrictMock<Lease::Api::IResourceRegistryMock> resourceRegistry;
        StrictMock<Lease::Api::ILeaseValidatorMock> leaseValidator;
        StrictMock<Pilot::Api::IControllerMock> manualController;
        StrictMock<Pilot::Api::IControllerMock> holdPositionController;
        EXPECT_CALL(resourceRegistry, RegisterResource(MakeExpectedControlResource()))
            .WillOnce(Return(Error::Api::Result<void>{}));
        EXPECT_CALL(leaseValidator, ValidateLease(MakeValidLeaseId(), MakeExpectedControlResourceId()))
            .WillOnce(Return(Error::Api::Result<Lease::Api::LeaseValidation>(Lease::Api::LeaseValidation{.IsValid = false})));
        Engine engine(resourceRegistry, leaseValidator, manualController, holdPositionController);

        const auto result = engine.Submit(MakeValidLeaseId(), Api::Input::OperatorCommand{});

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().Cause, make_error_code(Control::ErrorCode::InvalidControlLease));
    }
}
