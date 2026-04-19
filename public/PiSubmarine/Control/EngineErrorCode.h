#pragma once

#include <string>
#include <system_error>

namespace PiSubmarine::Control
{
    enum class EngineErrorCode
    {
        InvalidControlLease = 1,
        LeaseValidationFailed
    };

    class EngineErrorCategory final : public std::error_category
    {
    public:
        [[nodiscard]] const char* name() const noexcept override;
        [[nodiscard]] std::string message(int condition) const override;
    };

    [[nodiscard]] const std::error_category& GetEngineErrorCategory() noexcept;
    [[nodiscard]] std::error_code make_error_code(EngineErrorCode code) noexcept;
}

template <>
struct std::is_error_code_enum<PiSubmarine::Control::EngineErrorCode> : std::true_type
{
};
