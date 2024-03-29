#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <thread>

#include <SDL_log.h>

#include <Core/ConVar.hpp>
#include <Core/Engine.hpp>
#include <Core/Log.hpp>

struct ImGuiInputTextCallbackData;

namespace worlds
{
    class Console;
    extern Console* g_console;

    typedef std::function<void(const char* argString)> CommandFuncPtr;

    class Console
    {
    public:
        Console(bool openConsoleWindow, bool asyncStdinConsole = false);
        void registerCommand(CommandFuncPtr funcPtr, const char* name, const char* help);
        void drawWindow();
        void setShowState(bool show);
        void executeCommandStr(std::string cmdStr, bool log = true);
        ConVar* getConVar(const char* name)
        {
            return conVars.at(name);
        }
        ~Console();

    private:
        bool show;
        bool setKeyboardFocus;
        struct Command
        {
            CommandFuncPtr func;
            const char* name;
            const char* help;
            void* obj;
        };

        struct ConsoleMsg
        {
            SDL_LogPriority priority;
            std::string msg;
            int category;
            std::string dateTimeStr;
        };

        size_t historyPos;
        std::string currentCommand;
        std::vector<ConsoleMsg> msgs;
        std::vector<std::string> previousCommands;
        std::unordered_map<std::string, ConVar*> conVars;
        std::unordered_map<std::string, Command> commands;
        FILE* logFile;
        std::thread* asyncConsoleThread;
        bool asyncCommandReady;
        std::string asyncCommand;

        void logConsoleResponse();

        static int inputTextCallback(ImGuiInputTextCallbackData* data);
        static void logCallback(void* con, int category, SDL_LogPriority priority, const char* msg);
        void cmdHelp(const char* argString);
        void cmdExec(const char* argString);
        friend class ConVar;
        friend void asyncConsole();
    };
}
