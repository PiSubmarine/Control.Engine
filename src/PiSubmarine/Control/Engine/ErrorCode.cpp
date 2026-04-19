#include "PiSubmarine/Control/Engine/ErrorCode.h"

namespace PiSubmarine::Control
{
    namespace
    {
        const ErrorCategory ErrorCategoryInstance{};
    }

    const char* ErrorCategory::name() const noexcept
    {
        return "PiSubmarine.Control.Engine";
    }

    std::string ErrorCategory::message(const int condition) const
    {
        switch (static_cast<ErrorCode>(condition))
        {
            case ErrorCode::InvalidControlLease:
                return "control lease is invalid for the control resource";
            case ErrorCode::LeaseValidationFailed:
                return "control lease validation failed";
            default:
                return "unknown control engine error";
        }
    }

    const std::error_category& GetErrorCategory() noexcept
    {
        return ErrorCategoryInstance;
    }

    std::error_code make_error_code(const ErrorCode code) noexcept
    {
        return {static_cast<int>(code), GetErrorCategory()};
    }
}
