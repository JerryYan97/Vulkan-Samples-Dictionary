#pragma once
#include <string>
#include <unordered_map>
#include <any>
#include <list>

namespace SharedLib
{
    typedef std::unordered_map<size_t, std::any> HEventArguments; // Argument name -- Argument value

    class HEvent
    {
    public:
        explicit HEvent(const HEventArguments& arg, const std::string& type);
        ~HEvent() {};

        size_t GetEventType() const { return m_typeHash; }
        HEventArguments& GetArgs() { return m_arg; }

    private:
        HEventArguments m_arg;
        size_t m_typeHash;
        bool m_isHandled;
    };
}