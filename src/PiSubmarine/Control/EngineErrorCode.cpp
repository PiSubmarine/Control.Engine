#include "../../../public/PiSubmarine/Control/EngineErrorCode.h"

namespace PiSubmarine::Control
{
    namespace
    {
        const EngineErrorCategory ErrorCategoryInstance{};
    }

    const char* EngineErrorCategory::name() const noexcept
    {
        return "PiSubmarine.Control.Engine";
    }

    std::string EngineErrorCategory::message(const int condition) const
    {
        switch (static_cast<EngineErrorCode>(condition))
        {
            case EngineErrorCode::InvalidControlLease:
                return "control lease is invalid for the control resource";
            case EngineErrorCode::LeaseValidationFailed:
                return "control lease validation failed";
            default:
                return "unknown control engine error";
        }
    }

    const std::error_category& GetEngineErrorCategory() noexcept
    {
        return ErrorCategoryInstance;
    }

    std::error_code make_error_code(const EngineErrorCode code) noexcept
    {
        return {static_cast<int>(code), GetEngineErrorCategory()};
    }
}
