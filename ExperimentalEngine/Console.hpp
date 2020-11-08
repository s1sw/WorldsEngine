#pragma once
#include <string>
#include <unordered_map>
#include <SDL2/SDL_log.h>
#include "LogCategories.hpp"
#include <fstream>
#include <functional>

struct ImGuiInputTextCallbackData;

namespace worlds {
    class Console;
    extern Console* g_console;

    class ConVar {
    public:
        ConVar(const char* name, const char* defaultValue, const char* help = nullptr);
        ~ConVar();
        float getFloat() const { return parsedFloat; }
        int getInt() const { return parsedInt; }
        const char* getString() const { return value.c_str(); }
        const char* getName() const { return name; }
        const char* getHelp() const { return help; }
        void setValue(std::string newValue);
        operator float() const { return getFloat(); }
        operator int() const { return getInt(); }
    private:
        const char* help;
        const char* name;
        std::string value;
        int parsedInt;
        float parsedFloat;

        friend class Console;
    };

    typedef std::function<void(void* obj, const char* argString)> CommandFuncPtr;

    class Console {
    public:
        Console();
        void registerCommand(CommandFuncPtr funcPtr, const char* name, const char* help, void* obj);
        void drawWindow();
        void setShowState(bool show);
        void executeCommandStr(std::string cmdStr);
        ~Console();
    private:
        bool show;
        bool justOpened;
        struct Command {
            CommandFuncPtr func;
            const char* name;
            const char* help;
            void* obj;
        };

        struct ConsoleMsg {
            SDL_LogPriority priority;
            std::string msg;
            int category;
        };

        size_t historyPos;
        std::string currentCommand;
        std::vector<ConsoleMsg> msgs;
        std::vector<std::string> previousCommands;
        std::unordered_map<std::string, ConVar*> conVars;
        std::unordered_map<std::string, Command> commands;
        std::ofstream logFileStream;

        static int inputTextCallback(ImGuiInputTextCallbackData* data);
        static void logCallback(void* con, int category, SDL_LogPriority priority, const char* msg);
        static void cmdHelp(void* con, const char* argString);
        static void cmdExec(void* con, const char* argString);
        friend class ConVar;
    };
}