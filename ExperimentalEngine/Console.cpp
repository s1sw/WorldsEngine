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
// Just for OutputDebugStringA
#define _AMD64_
#include <debugapi.h>
#endif
#include "Log.hpp"
#include <thread>

namespace worlds {
    // Because static initialisation is the first thing that occurs when the game is started,
    // any static convars will fail to find the console global. To fix this, we create a linked list of convars
    // that's added to the console when it is initialised.
    struct ConvarLink {
        ConVar* var;
        ConvarLink* next;
    };
    ConvarLink* firstLink = nullptr;
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
        { WELogCategoryApp, "Application" },
        { WELogCategoryScripting, "Scripting" },
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
        : help(help) 
        , name(name)
        , value(defaultValue) {

        parsedInt = std::stoi(value);
        parsedFloat = (float)std::stof(value);

        if (g_console == nullptr) {
            ConvarLink* thisLink = new ConvarLink{ this, nullptr };
            if (firstLink) {
                ConvarLink* next = firstLink;
                while (next->next != nullptr) {
                    next = next->next;
                }

                next->next = thisLink;
            } else {
                firstLink = thisLink;
            }
        } else {
            std::string nameLower = name;
            for (auto& c : nameLower)
                c = std::tolower(c);

            g_console->conVars.insert({ nameLower, this });
        }

    }

    void ConVar::setValue(std::string value) {
        this->value = value;
        parsedInt = std::stoi(value);
        parsedFloat = (float)std::stof(value);
    }

    ConVar::~ConVar() {
        if (g_console)
            g_console->conVars.erase(name);
    }

    void asyncConsole() {
    }

    Console::Console(bool asyncStdinConsole)
        : show(false)
        , setKeyboardFocus(false) 
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

        if (firstLink != nullptr) {
            ConvarLink* curr = firstLink;

            while (curr != nullptr) {
                ConvarLink* next = curr->next;
                std::string nameLower = curr->var->name;
                for (auto& c : nameLower) {
                    c = std::tolower(c);
                }

                conVars.insert({ nameLower, curr->var });
                delete curr;
                curr = next;
            }
        }

        if (asyncStdinConsole) {
            asyncConsoleThread = new std::thread(asyncConsole);
        }
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

    bool strCompare(const char* a, const char* b) { return strcmp(a, b) < 0; }

    int Console::inputTextCallback(ImGuiInputTextCallbackData* data) {
        static int completionPos = 0;
        static bool completionsCached = false;
        static std::vector<const char*> completions;

        switch (data->EventFlag) {
        case ImGuiInputTextFlags_CallbackHistory:
        {
            if (g_console->previousCommands.size() == 0) return 0;

            if (data->EventKey == ImGuiKey_UpArrow) {
                g_console->historyPos--;
            }

            if (data->EventKey == ImGuiKey_DownArrow) {
                g_console->historyPos++;
            }

            g_console->historyPos = std::clamp(g_console->historyPos, (size_t)0, g_console->previousCommands.size() - 1);

            auto historyCmd = g_console->previousCommands[g_console->historyPos];
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, historyCmd.c_str());
            break;
        }
        case ImGuiInputTextFlags_CallbackCompletion:
        {
            if (!completionsCached) {
                completionPos = 0;
                completions.clear();
                std::string lowerified = data->Buf;
                for (auto& c : lowerified)
                    c = std::tolower(c);

                // search for commands
                for (auto& pair : g_console->commands) {
                    if (pair.first.starts_with(lowerified)) {
                        completions.push_back(pair.second.name);
                    }
                }

                // search for convars
                for (auto& pair : g_console->conVars) {
                    if (pair.first.starts_with(lowerified)) {
                        completions.push_back(pair.second->name);
                    }
                }

                std::sort(completions.begin(), completions.end(), strCompare);

                completionsCached = true;

                if (completions.size() > 1) {
                    g_console->msgs.push_back(ConsoleMsg{ SDL_LOG_PRIORITY_INFO, "completions: ", CONSOLE_RESPONSE_CATEGORY });
                    for (auto& completion : completions) {
                        g_console->msgs.push_back(ConsoleMsg{ SDL_LOG_PRIORITY_INFO, completion, CONSOLE_RESPONSE_CATEGORY });
                    }
                }
            } else {
                completionPos++;

                if ((size_t)completionPos >= completions.size())
                    completionPos = 0;
            }

            if (!completions.empty()) {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, completions[completionPos]);
            }
            break;
        }
        case ImGuiInputTextFlags_CallbackEdit:
        {
            completionsCached = false;
            break;
        }
        }
        return 0;
    }

    void Console::drawWindow() {
        if (ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_GRAVE] == 0.0f) {
            show = !show;
        }

        if (!show) {
            setKeyboardFocus = true;
            return;
        }

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

                if ((size_t)currMsgIdx == msgs.size() - 1 && msgs.size() != (size_t)lastMsgCount) {
                    ImGui::SetScrollHereY();
                    lastMsgCount = (int)msgs.size();
                }

                currMsgIdx++;
            }

            ImGui::EndChild();

            if (setKeyboardFocus)
                ImGui::SetKeyboardFocusHere();
            bool executeCmd = ImGui::InputText("##command", &currentCommand, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackEdit, inputTextCallback);

            ImGui::SameLine();
            executeCmd |= ImGui::Button("Submit");

            setKeyboardFocus = false;

            if (executeCmd) {
                executeCommandStr(currentCommand);
                previousCommands.push_back(currentCommand);
                currentCommand.clear();
                historyPos = previousCommands.size();
                setKeyboardFocus = true;
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
                msgs.push_back(ConsoleMsg{ SDL_LOG_PRIORITY_INFO, cmdStr, CONSOLE_RESPONSE_CATEGORY });
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
                std::string valStr = argString;

                if (argString[0] == '=')
                    valStr = valStr.substr(1);

                (*convarPos).second->setValue(valStr.c_str());
                logMsg(CONSOLE_RESPONSE_CATEGORY, "%s = %s", cmdString.c_str(), valStr.c_str());
            } else if (cmdPos != commands.end()) {
                logMsg(CONSOLE_RESPONSE_CATEGORY, cmdStr.c_str());
                (*cmdPos).second.func((*cmdPos).second.obj, argString.c_str());
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

#ifndef _WIN32
        if (priority == SDL_LOG_PRIORITY_WARN)
            std::cout << "\033[33m";
        if (priority == SDL_LOG_PRIORITY_ERROR)
            std::cout << "\033[31m";
        if (priority == SDL_LOG_PRIORITY_DEBUG || priority ==SDL_LOG_PRIORITY_VERBOSE)
            std::cout << "\033[36m";
#endif
        std::cout << outStr << "\n";
#ifndef _WIN32
        std::cout << "\033[0m";
#endif

        // Use std::endl for the file to force a flush
        if (con->logFileStream.good())
            con->logFileStream << outStr << std::endl;

        con->msgs.push_back(ConsoleMsg{ priority, msg, category });

#ifdef _WIN32
        OutputDebugStringA(outStr.c_str());
        OutputDebugStringA("\n");
#endif
    }

    Console::~Console() {
        asyncConsoleThread->join();
        delete asyncConsoleThread;
        logFileStream << "[" << getDateTimeString() << "]" << "Closing log file." << std::endl;
        logFileStream.close();
        g_console = nullptr;
    }
}
