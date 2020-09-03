#include "Console.hpp"
#include "Fatal.hpp"
#include <SDL2/SDL_log.h>
#include <chrono>
#include <ctime>
#include "imgui.h"
#include "imgui_stdlib.h"
#include <iostream>
#include "Engine.hpp"
#include "IOUtil.hpp"
#include <sstream>
#include "LogCategories.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS
#define NOCRYPT
#define NOVIRTUALKEYCODES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NORASTEROPS
#define NOSYSCOMMANDS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCOLOR
#define NODRAWTEXT
#define NOGDI
#define NOKERNEL
#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#include <Windows.h>
#endif

namespace worlds {
    const int CONSOLE_RESPONSE_CATEGORY = 255;
    Console* g_console;

    const std::unordered_map<SDL_LogPriority, ImColor> priorityColors = {
        { SDL_LOG_PRIORITY_CRITICAL, ImColor(0.8f, 0.0f, 0.0f) },
        { SDL_LOG_PRIORITY_DEBUG, ImColor(1.0f, 1.0f, 1.0f, 0.5f) },
        { SDL_LOG_PRIORITY_INFO, ImColor(1.0f, 1.0f, 1.0f) },
        { SDL_LOG_PRIORITY_VERBOSE, ImColor(1.0f, 1.0f, 1.0f, 0.75f) },
        { SDL_LOG_PRIORITY_WARN, ImColor(1.0f, 1.0f, 0.0f) },
        { SDL_LOG_PRIORITY_ERROR, ImColor(1.0f, 0.0f, 0.0f) }
    };

    const std::unordered_map<int, const char*> categories = {
        { SDL_LOG_CATEGORY_APPLICATION, "SDL Application" },
        { SDL_LOG_CATEGORY_AUDIO, "SDL Audio" },
        { SDL_LOG_CATEGORY_ASSERT, "SDL Assert" },
        { SDL_LOG_CATEGORY_ERROR, "SDL Error" },
        { SDL_LOG_CATEGORY_INPUT, "SDL Input" },
        { SDL_LOG_CATEGORY_RENDER, "SDL Render" },
        { WELogCategoryEngine, "Engine" },
        { WELogCategoryAudio, "Audio" },
        { WELogCategoryRender, "Render" },
        { WELogCategoryUI, "UI" },
        { WELogCategoryApp, "Application"},
        { CONSOLE_RESPONSE_CATEGORY, "Console Response" }
    };

    const std::unordered_map<SDL_LogPriority, const char*> priorities = {
        { SDL_LOG_PRIORITY_CRITICAL, "Critical" },
        { SDL_LOG_PRIORITY_DEBUG, "Debug" },
        { SDL_LOG_PRIORITY_INFO, "Info" },
        { SDL_LOG_PRIORITY_VERBOSE, "Verbose" },
        { SDL_LOG_PRIORITY_WARN, "Warning" },
        { SDL_LOG_PRIORITY_ERROR, "Error" }
    };

    ConVar::ConVar(const char* name, const char* defaultValue, const char* help)
        : value(defaultValue)
        , name(name)
        , help(help) {
        if (g_console == nullptr) {
            fatalErr("A ConVar was created before the console! If you're a developer,\nyou've messed up (likely a static or global convar). If you're not a developer, your installation is corrupted.");
            return;
        }

        std::string nameLower = name;
        for (auto& c : nameLower)
            c = std::tolower(c);
        g_console->conVars.insert({ nameLower, this });

        parsedInt = std::atoi(value.c_str());
        parsedFloat = std::atof(value.c_str());
    }

    void ConVar::setValue(std::string value) {
        this->value = value;
        parsedInt = std::atoi(value.c_str());
        parsedFloat = std::atof(value.c_str());
    }

    ConVar::~ConVar() {
        SDL_Log("ConVar %s destroyed", name);
        g_console->conVars.erase(name);
    }

    Console::Console()
        : show(false)
        , logFileStream("converge.log") {
        g_console = this;
        SDL_LogSetOutputFunction(logCallback, this);
        SDL_LogSetPriority(CONSOLE_RESPONSE_CATEGORY, SDL_LOG_PRIORITY_INFO);
        SDL_LogSetPriority(WELogCategoryUI, SDL_LOG_PRIORITY_INFO);
        registerCommand(cmdHelp, "help", "Displays help about all commands.", this);
        registerCommand(cmdExec, "exec", "Executes a command file.", this);

        for (auto& cPair : categories) {
#ifndef NDEBUG
            SDL_LogSetPriority(cPair.first, SDL_LOG_PRIORITY_VERBOSE);
#else
            SDL_LogSetPriority(cPair.first, SDL_LOG_PRIORITY_INFO);
#endif
        }

#ifdef _WIN32
        notepadHwnd = FindWindowA(NULL, "Untitled - Notepad");
        if (notepadHwnd == NULL)
            notepadHwnd = FindWindowA(NULL, "*Untitled - Notepad");
#endif
    }

    void Console::registerCommand(CommandFuncPtr cmd, const char* name, const char* help, void* obj) {
        std::string nameLower = name;
        for (auto& c : nameLower)
            c = std::tolower(c);
        commands.insert({ nameLower, Command { cmd, name, help, obj } });
    }

    void Console::cmdHelp(void* con, const char* argString) {
        auto* console = reinterpret_cast<Console*>(con);
        SDL_LogInfo(CONSOLE_RESPONSE_CATEGORY, "Commands:");
        for (auto& cmd : console->commands) {
            SDL_LogInfo(CONSOLE_RESPONSE_CATEGORY, "%s: %s", cmd.second.name, cmd.second.help);
        }

        SDL_LogInfo(CONSOLE_RESPONSE_CATEGORY, "ConVars:");

        for (auto& conVar : console->conVars) {
            if (conVar.second->getHelp() == nullptr)
                SDL_LogInfo(CONSOLE_RESPONSE_CATEGORY, "%s=%s", conVar.second->getName(), conVar.second->getString());
            else {
                SDL_LogInfo(CONSOLE_RESPONSE_CATEGORY, "%s=%s (%s)", conVar.second->getName(), conVar.second->getString(), conVar.second->getHelp());
            }
        }
    }

    void Console::cmdExec(void* con, const char* argString) {
        auto* console = reinterpret_cast<Console*>(con);

        auto loadRes = LoadFileToString(argString + std::string(".txt"));

        if (loadRes.error != IOError::None) {
            SDL_LogError(CONSOLE_RESPONSE_CATEGORY, "Can't find %s", argString);
            return;
        }

        // Account for Windows's silly line endings
        loadRes.value.erase(std::remove(loadRes.value.begin(), loadRes.value.end(), '\r'), loadRes.value.end());

        std::stringstream stream(loadRes.value);
        std::string line;

        while (std::getline(stream, line)) {
            console->executeCommandStr(line);
        }
    }

    void Console::drawWindow() {
        if (ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_GRAVE] == 0.0f) {
            show = !show;
            //g_engine->getInputSystem().setMouseLockState(!show);
        }

        if (!show)
            return;

        static int lastMsgCount = 0;
        if (ImGui::Begin("Console", &show)) {
            if (ImGui::Button("Clear")) {
                msgs.clear();
            }
            ImGui::BeginChild("ConsoleScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

            int currMsgIdx = 0;
            for (auto& msg : msgs) {
                ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)priorityColors.at(msg.priority));
                if (msg.category != CONSOLE_RESPONSE_CATEGORY)
                    ImGui::TextUnformatted(("[" + std::string(categories.at(msg.category)) + "] " + "[" + std::string(priorities.at(msg.priority)) + "] " + msg.msg).c_str());
                else
                    ImGui::TextUnformatted(msg.msg.c_str());
                ImGui::PopStyleColor();

                if (currMsgIdx == msgs.size() - 1 && msgs.size() != lastMsgCount) {
                    ImGui::SetScrollHereY();
                    lastMsgCount = msgs.size();
                }

                currMsgIdx++;
            }

            ImGui::EndChild();

            bool executeCmd = ImGui::InputText("##command", &currentCommand, ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            executeCmd |= ImGui::Button("Submit");

            if (executeCmd) {
                executeCommandStr(currentCommand);
                currentCommand.clear();
            }

            ImGui::SetItemDefaultFocus();
        }
        ImGui::End();
    }

    void Console::setShowState(bool show) {
        this->show = show;
    }

    void Console::executeCommandStr(std::string cmdStr) {
        size_t firstSpace = cmdStr.find_first_of(' ');
        if (firstSpace == std::string::npos) {
            for (auto& c : cmdStr)
                c = std::tolower(c);
            auto convarPos = conVars.find(cmdStr);
            auto cmdPos = commands.find(cmdStr);

            if (cmdPos != commands.end()) {
                msgs.push_back(ConsoleMsg{ SDL_LOG_PRIORITY_INFO, (*cmdPos).second.name, CONSOLE_RESPONSE_CATEGORY });
                (*cmdPos).second.func((*cmdPos).second.obj, "");
            } else if (convarPos != conVars.end()) {
                msgs.push_back(ConsoleMsg{ SDL_LOG_PRIORITY_INFO, (*convarPos).second->getName() + std::string("=") + (*convarPos).second->getString(), CONSOLE_RESPONSE_CATEGORY });
            } else {
                msgs.push_back(ConsoleMsg{ SDL_LOG_PRIORITY_ERROR, "No command/convar named " + cmdStr, CONSOLE_RESPONSE_CATEGORY });
            }
        } else {
            std::string argString = cmdStr.substr(firstSpace + 1);
            std::string cmdString = cmdStr.substr(0, firstSpace);
            for (auto& c : cmdString)
                c = std::tolower(c);

            auto convarPos = conVars.find(cmdString);
            auto cmdPos = commands.find(cmdString);

            if (convarPos != conVars.end()) {
                (*convarPos).second->setValue(argString.c_str());
                logMsg(CONSOLE_RESPONSE_CATEGORY, "%s = %s", cmdString.c_str(), argString.c_str());
            } else if (cmdPos != commands.end()) {
                (*cmdPos).second.func((*cmdPos).second.obj, argString.c_str());
                logMsg(CONSOLE_RESPONSE_CATEGORY, cmdString.c_str());
            } else {
                msgs.push_back(ConsoleMsg{ SDL_LOG_PRIORITY_ERROR, "No command/convar named " + cmdString, CONSOLE_RESPONSE_CATEGORY });
            }
        }
    }

    std::string getDateTimeString() {
        auto dt = std::chrono::system_clock::now();
        std::time_t dt2 = std::chrono::system_clock::to_time_t(dt);
        char ts[64];
        strftime(ts, 64, "%H:%M:%S", localtime(&dt2));
        return std::string(ts);
    }

    void Console::logCallback(void* conVP, int category, SDL_LogPriority priority, const char* msg) {
        Console* con = (Console*)conVP;
        std::string outStr =
            "[" + getDateTimeString() + "]"
            + "[" + categories.at(category) + "]"
            + "[" + priorities.at(priority) + "] "
            + msg;

        std::cout << outStr << "\n";

        // Use std::endl for the file to force a flush
        if (con->logFileStream.good())
            con->logFileStream << outStr << std::endl;

        con->msgs.push_back(ConsoleMsg{ priority, msg, category });

#ifdef _WIN32
        OutputDebugStringA(outStr.c_str());
        OutputDebugStringA("\n");

        HWND edit;
        edit = FindWindowExA((HWND)con->notepadHwnd, NULL, "EDIT", NULL);
        SendMessageA(edit, EM_REPLACESEL, TRUE, (LPARAM)(outStr + "\n").c_str());
#endif
    }

    Console::~Console() {
        logFileStream << "[" << getDateTimeString() << "]" << "Closing log file." << std::endl;
        logFileStream.close();
    }
}