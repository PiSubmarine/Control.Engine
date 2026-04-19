# Control.Engine

`PiSubmarine.Control.Engine` is the canonical orchestration runtime for operator control.

## Responsibility

`Engine`:

- registers the control resource in `Lease.Api`
- validates the control lease carried by each submitted `OperatorCommand`
- receives raw `Control.Api::Input::OperatorCommand` updates through `Control.Api::Input::ISink`
- latches optional operator-controlled settings
- interprets `Mode::Request`
- selects the currently active `Control.Pilot.Api::IController`
- forwards resolved `Control.Pilot.Api::Input` to that pilot
- drives the active pilot through `Time::ITickable`

## Threading

`Submit(...)` is thread-safe and may be called from a transport-facing thread.

`Tick(...)` must be called from the single deterministic control thread.

Cross-thread communication happens through an internal latest-command mailbox. The ticked pilot-facing state is only
mutated on the tick thread.

## Current Scope

The first implementation supports the two pilot modes currently represented in `Control.Api::Input::Mode::Request`:

- `Manual`
- `HoldPosition`

Future modes can be added by extending the mode-selection logic and constructor dependencies.
