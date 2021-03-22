#include "Console.hpp"
#include "Fatal.hpp"
#include <SDL_log.h>
#include <chrono>
#include <ctime>
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_stdlib.h"
#include <iostream>
#include "Engine.hpp"
#include "../IO/IOUtil.hpp"
#include <sstream>
#include "LogCategories.hpp"
#ifdef _WIN32
#define _AMD64_
#include <debugapi.h>
#include <ConsoleApi.h>
#include <wtypes.h>
#include <WinBase.h>
#endif
#include "Log.hpp"
#include <thread>
#include "Fatal.hpp"

namespace worlds {
    // Because static initialisation is the first thing that occurs when the game is started,
    // any static convars will fail to find the console global. To fix this, we create a linked list of convars
    // that's added to the console when it is initialised.
    struct ConvarLink {
        ConVar* var;
        ConvarLink* next;
    };

    ConVar logToStdout { "logToStdOut",
#ifdef NDEBUG
        "0",
#else
        "1",
#endif
        "Log to stdout in addition to the file and the console." };

    ConvarLink* firstLink = nullptr;
    const int CONSOLE_RESPONSE_CATEGORY = 255;
    Console* g_console;

    const std::unordered_map<SDL_LogPriority, ImColor> priorityColors = {
        { SDL_LOG_PRIORITY_CRITICAL, ImColor(0.8f, 0.0f, 0.0f) },
        { SDL_LOG_PRIORITY_DEBUG, ImColor(0.75f, 0.75f, 0.75f, 1.0f) },
        { SDL_LOG_PRIORITY_INFO, ImColor(0.47f, 0.57f, 1.0f, 1.0f) },
        { SDL_LOG_PRIORITY_VERBOSE, ImColor(0.75f, 0.75f, 0.75f, 1.0f) },
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
        { WELogCategoryPhysics, "Physics" },
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
        try {
            parsedInt = std::stoi(value);
            parsedFloat = (float)std::stof(value);
            this->value = value;
        } catch (const std::invalid_argument&) {
            logErr("Invalid convar value %s", value.c_str());
        }
    }

    ConVar::~ConVar() {
        if (g_console)
            g_console->conVars.erase(name);
    }


    static std::string asyncCmdStr;

    void asyncConsole() {
        while (g_console) {
            char c = '\0';
            asyncCmdStr.clear();
            while(g_console) {
                std::cin.get(c);
                if (c == '\n')
                    break;
                asyncCmdStr += c;
            }

            std::cin.clear();

            g_console->asyncCommand = asyncCmdStr;
            g_console->asyncCommandReady = true;
            asyncCmdStr.clear();
        }
    }

    const bool ENABLE_SECONDARY_SCREEN = false;

    void goToSecondaryScreen() {
        if (!ENABLE_SECONDARY_SCREEN) return;
        std::cout << "\033[7\033[?47h";
    }

    void returnToPrimary() {
        if (!ENABLE_SECONDARY_SCREEN) return;
        std::cout << "\033[2J\033[?47l\0338";
    }

    bool enableVT100 = true;

    Console::Console(bool asyncStdinConsole)
        : show(false)
        , setKeyboardFocus(false)
        , logFileStream("worldsengine.log")
        , asyncConsoleThread(nullptr)
        , asyncCommandReady(false) {
        g_console = this;
        SDL_LogSetOutputFunction(logCallback, this);
        registerCommand(cmdHelp, "help", "Displays help about all commands.", this);
        registerCommand(cmdExec, "exec", "Executes a command file.", this);

        for (auto& cPair : categories) {
//#ifndef NDEBUG
            SDL_LogSetPriority(cPair.first, SDL_LOG_PRIORITY_VERBOSE);
//#else
//            SDL_LogSetPriority(cPair.first, SDL_LOG_PRIORITY_WARN);
//#endif
        }

        SDL_LogSetPriority(CONSOLE_RESPONSE_CATEGORY, SDL_LOG_PRIORITY_INFO);

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

#ifndef NDEBUG
#ifdef _WIN32
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            if (!AllocConsole()) {
                logErr("failed to allocconsole, assuming one already exists");
            }
            auto pid = GetCurrentProcessId();
            std::string s = "Worlds Engine (PID " + std::to_string(pid) + ")";
            SetConsoleTitleA(s.c_str());
        }

        FILE *fDummy;
        freopen_s(&fDummy, "CONIN$", "r", stdin);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
        freopen_s(&fDummy, "CONOUT$", "w", stdout);

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) {
            logWarn("Failed to get console output handle");
            enableVT100 = false;
            return;
        }

        DWORD dwMode = 0;
        if (!GetConsoleMode(hOut, &dwMode)) {
            logWarn("Failed to get console mode");
            enableVT100 = false;
            return;
        }

        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (!SetConsoleMode(hOut, dwMode)) {
            logWarn("Failed to set console mode");
            enableVT100 = false;
            return;
        }

        std::cin.clear();
        std::cout.clear();
#endif
#endif

        if (asyncStdinConsole) {
            asyncConsoleThread = new std::thread(asyncConsole);
            goToSecondaryScreen();
        }

        registerCommand([](void*, const char* arg) {
            logMsg("%s", arg);
        }, "echo", "Echos the argument to the screen.", nullptr);
    }

    void Console::registerCommand(CommandFuncPtr cmd, const char* name, const char* help, void* obj) {
        std::string nameLower = name;
        for (auto& c : nameLower)
            c = std::tolower(c);
        commands.insert({ nameLower, Command { cmd, name, help, obj } });
    }

    void Console::cmdHelp(void* con, const char*) {
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

        bool printToConsole = true;
        while (std::getline(stream, line)) {
            if (line[0] == '#') {
                continue;
            } else if (line == "nolog") {
                printToConsole = false;
            }

            console->executeCommandStr(line, printToConsole);
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
        if (asyncCommandReady) {
            executeCommandStr(asyncCommand);
            asyncCommandReady = false;
        }

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

            if (ImGui::BeginChild("ConsoleScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()))) {
                float scrollRegionY = ImGui::GetContentRegionAvail().y;
                int currMsgIdx = 0;
                float scroll = ImGui::GetScrollY();
                float scrollMax = scroll + scrollRegionY;
                float cHeight = 0.0f;
                float lineHeight = ImGui::GetTextLineHeightWithSpacing();
                for (auto& msg : msgs) {
                    if ((cHeight + lineHeight) > scroll && (cHeight - lineHeight) < scrollMax) {
                        ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)priorityColors.at(msg.priority));
                        if (msg.category != CONSOLE_RESPONSE_CATEGORY)
                            ImGui::TextUnformatted(("[" + std::string(categories.at(msg.category)) + "] " + "[" + std::string(priorities.at(msg.priority)) + "] " + msg.msg).c_str());
                        else
                            ImGui::TextUnformatted(msg.msg.c_str());
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::NewLine();
                    }

                    cHeight += lineHeight;
                    currMsgIdx++;
                }

                if (lastMsgCount != (int)msgs.size()) {
                    lastMsgCount = msgs.size();
                    ImGui::SetScrollY(cHeight);
                }

                ImGui::EndChild();
            }

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

    void Console::executeCommandStr(std::string cmdStr, bool log) {
        size_t firstSpace = cmdStr.find_first_of(' ');
        if (firstSpace == std::string::npos) {
            for (auto& c : cmdStr)
                c = std::tolower(c);
            auto convarPos = conVars.find(cmdStr);
            auto cmdPos = commands.find(cmdStr);

            if (cmdPos != commands.end()) {
                if (log)
                    msgs.push_back(ConsoleMsg{ SDL_LOG_PRIORITY_INFO, cmdStr, CONSOLE_RESPONSE_CATEGORY });
                (*cmdPos).second.func((*cmdPos).second.obj, "");
            } else if (convarPos != conVars.end()) {
                if (log)
                    logMsg(CONSOLE_RESPONSE_CATEGORY, "%s=%s", (*convarPos).second->getName(), (*convarPos).second->getString());
            } else {
                logErr(CONSOLE_RESPONSE_CATEGORY, "No command/convar named %s", cmdStr.c_str());
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
                if (log)
                    logMsg(CONSOLE_RESPONSE_CATEGORY, "%s = %s", cmdString.c_str(), valStr.c_str());
            } else if (cmdPos != commands.end()) {
                if (log)
                    msgs.push_back(ConsoleMsg{ SDL_LOG_PRIORITY_INFO, cmdStr, CONSOLE_RESPONSE_CATEGORY });
                (*cmdPos).second.func((*cmdPos).second.obj, argString.c_str());
            } else {
                if (log)
                    logErr(CONSOLE_RESPONSE_CATEGORY, "No command/convar named %s", cmdString.c_str());
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

        auto col = priorityColors.at(priority);
        int r = (int)(col.Value.x * 255);
        int g = (int)(col.Value.y * 255);
        int b = (int)(col.Value.z * 255);

        if (logToStdout.getInt()) {
            if (enableVT100) {
                std::string colorStr = std::string("\033[38;2;")
                          + std::to_string(r)
                    + ";" + std::to_string(g)
                    + ";" + std::to_string(b)
                    + "m";

                const char* clearLineCode = "\033[2K";

                std::cout << "\r" << clearLineCode << colorStr << outStr << "\n";
                if (g_console->asyncConsoleThread)
                    std::cout << "\033[32mserver> \033[0m";
                else
                    std::cout << "\033[0m";
                std::cout.flush();
            } else {
                std::cout << outStr << "\n";
            }
        }

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
        logFileStream << "[" << getDateTimeString() << "]" << "Closing log file." << std::endl;
        logFileStream.close();
        g_console = nullptr;
        returnToPrimary();

        if (asyncConsoleThread) {
            // here we're just detaching the thread and leaving it to its fate.
            // this could potentially cause issues in the future, but i'm not sure
            asyncConsoleThread->detach();
            delete asyncConsoleThread;
        }

    }
}
