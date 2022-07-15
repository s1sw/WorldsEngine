#pragma once
#include <string>

namespace worlds
{
    class ConVar
    {
      public:
        ConVar(const char *name, const char *defaultValue, const char *help = nullptr);
        ~ConVar();
        float getFloat() const
        {
            return parsedFloat;
        }
        int getInt() const
        {
            return parsedInt;
        }
        const char *getString() const
        {
            return value.c_str();
        }
        const char *getName() const
        {
            return name;
        }
        const char *getHelp() const
        {
            return help;
        }
        void setValue(std::string newValue);
        operator float() const
        {
            return getFloat();
        }
        operator bool() const
        {
            return (bool)getInt();
        }

      private:
        const char *help;
        const char *name;
        std::string value;
        int parsedInt;
        float parsedFloat;

        friend class Console;
    };
}
