#include "Engine.hpp"
#include <robin_hood.h>

namespace worlds
{
    robin_hood::unordered_map<std::string, std::string> args;

    bool isSwitch(char *arg)
    {
        return arg[0] == '-' && arg[1] == '-';
    }

    void EngineArguments::parseArguments(int argc, char **argv)
    {
        for (int i = 1; i < argc; i++)
        {
            size_t len = strlen(argv[i]);

            if (len <= 2)
                continue;
            if (!isSwitch(argv[i]))
                continue;

            char *arg = argv[i] + 2;
            const char *val = nullptr;

            if (i < argc - 1 && !isSwitch(argv[i + 1]))
                val = argv[i + 1];
            else
                val = "";

            addArgument(arg, val);
        }
    }

    void EngineArguments::addArgument(const char *arg, const char *value)
    {
        args.insert({arg, value});
    }

    bool EngineArguments::hasArgument(const char *arg)
    {
        return args.contains(arg);
    }

    std::string_view EngineArguments::argumentValue(const char *arg)
    {
        return args.at(arg);
    }
}
