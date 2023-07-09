#include "Event.h"
#include "MathUtils.h"

namespace SharedLib
{
    // ================================================================================================================
    HEvent::HEvent(
        const HEventArguments& arg,
        const std::string& type)
        : m_isHandled(false)
    {
        m_typeHash = crc32(type.data());
        m_arg = arg;
    }
}