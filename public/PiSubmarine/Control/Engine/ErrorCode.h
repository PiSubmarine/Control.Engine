#pragma once

#include <string>
#include <system_error>

namespace PiSubmarine::Control
{
    enum class ErrorCode
    {
        InvalidControlLease = 1,
        LeaseValidationFailed
    };

    class ErrorCategory final : public std::error_category
    {
    public:
        [[nodiscard]] const char* name() const noexcept override;
        [[nodiscard]] std::string message(int condition) const override;
    };

    [[nodiscard]] const std::error_category& GetErrorCategory() noexcept;
    [[nodiscard]] std::error_code make_error_code(ErrorCode code) noexcept;
}

template <>
struct std::is_error_code_enum<PiSubmarine::Control::ErrorCode> : std::true_type
{
};
