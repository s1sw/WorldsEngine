#pragma once
#include <Editor/Editor.hpp>
#include <Editor/Widgets/LogoWidget.hpp>
#include <IO/IOUtil.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <slib/Path.hpp>
#include <slib/Win32Util.hpp>
#include <sstream>

namespace worlds
{
    class EditorStartScreen
    {
      public:
        EditorStartScreen(const EngineInterfaces& interfaces) : interfaces(interfaces)
        {
        }

        void draw(Editor* ed)
        {
            struct RecentProject
            {
                std::string name;
                std::string path;
                ImTextureID badge = nullptr;
            };

            static std::vector<RecentProject> recentProjects;
            static bool loadedRecentProjects = false;
            static LogoWidget logo{interfaces};

            if (!loadedRecentProjects)
            {
                loadedRecentProjects = true;
                char* prefPath = SDL_GetPrefPath("Someone Somewhere", "Worlds Engine");
                std::ifstream recentProjectsStream(prefPath + std::string{"recentProjects.txt"});

                if (recentProjectsStream.good())
                {
                    std::string currLine;

                    while (std::getline(recentProjectsStream, currLine))
                    {
                        try
                        {
                            nlohmann::json pj = nlohmann::json::parse(std::ifstream(currLine));
                            RecentProject rp{pj["projectName"], currLine};

                            std::filesystem::path badgePath =
                                std::filesystem::path(currLine).parent_path() / "Editor" / "badge.png";
                            if (std::filesystem::exists(badgePath))
                            {
                            }

                            recentProjects.push_back({pj["projectName"], currLine});
                        }
                        catch (nlohmann::detail::exception except)
                        {
                            logWarn("Failed to parse line of recent project file.");
                        }
                    }
                }

                SDL_free(prefPath);
            }

            ImVec2 menuBarSize;
            if (ImGui::BeginMainMenuBar())
            {
                menuBarSize = ImGui::GetWindowSize();
                ImGui::EndMainMenuBar();
            }

            ImGuiViewport* viewport = ImGui::GetMainViewport();
            glm::vec2 projectWinSize = (glm::vec2)viewport->Size - glm::vec2(0.0f, menuBarSize.y);
            ImGui::SetNextWindowPos((glm::vec2)viewport->Pos + glm::vec2(0.0f, menuBarSize.y));
            ImGui::SetNextWindowSize(projectWinSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin("ProjectWindow", 0,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoBringToFrontOnFocus);
            ImGui::PopStyleVar(2);

            glm::vec2 initialCpos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                initialCpos + glm::vec2(490.0f, 0.0f),
                initialCpos + glm::vec2(490.0f, projectWinSize.y - ImGui::GetStyle().WindowPadding.y * 2.0f),
                ImGui::GetColorU32(ImGuiCol_Separator), 2.0f);

            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 500.0f);

            logo.draw();
            ImGui::Text("Select a project.");

            bool open = true;
            if (ImGui::BeginPopupModal("Create Project", &open))
            {
                static std::string projectName;
                ImGui::InputText("Name", &projectName);

                static bool showSpaceWarning = false;

                if (showSpaceWarning)
                {
                    ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "The project name cannot contain spaces!");
                }

                static slib::String projectPath;
                ImGui::InputText("Path", &projectPath);
                ImGui::SameLine();

                if (ImGui::Button("Choose Folder"))
                {
                    projectPath = slib::Win32Util::openFolder();
                }

                if (ImGui::Button("Create"))
                {
                    if (projectName.contains(' '))
                    {
                        showSpaceWarning = true;
                    }
                    else
                    {
                        // Create the thing!
                        std::string projectDir = (projectPath).cStr() + ("/" + projectName);
                        std::filesystem::copy("./ProjectTemplate", projectDir,
                                              std::filesystem::copy_options::recursive);
                        templateFile(projectDir + "/WorldsProject.json", projectName);
                        templateFile(projectDir + "/Code/ExampleSystem.cs", projectName);
                        templateFile(projectDir + "/Code/FreecamSystem.cs", projectName);
                        templateFile(projectDir + "/Code/Game.csproj", projectName);
                        ed->openProject(projectDir + "/WorldsProject.json");
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndPopup();
            }

            if (ImGui::Button("Open"))
            {
                slib::String path = slib::Win32Util::openFile("WorldsProject.json");

                if (!path.empty())
                {
                    ed->openProject(path.cStr());
                }
            }

            if (ImGui::Button("Create New"))
            {
                ImGui::OpenPopup("Create Project");
            }

            ImGui::NextColumn();

            for (RecentProject& project : recentProjects)
            {
                const glm::vec2 widgetSize{600.0f, 200.0f};
                ImGui::PushID(project.path.c_str());

                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRect(ImGui::GetCursorScreenPos(), (glm::vec2)ImGui::GetCursorScreenPos() + widgetSize,
                            ImColor(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg)), 5.0f, 0, 3.0f);

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 5.0f));
                if (ImGui::BeginChild(ImGui::GetID("project"), widgetSize, true,
                                      ImGuiWindowFlags_AlwaysUseWindowPadding))
                {
                    pushBoldFont();
                    ImGui::TextUnformatted(project.name.c_str());
                    ImGui::PopFont();
                    ImGui::SameLine();
                    ImGui::TextUnformatted(project.path.c_str());

                    if (ImGui::Button("Open Project"))
                    {
                        ed->openProject(project.path);
                    }
                }
                ImGui::EndChild();

                ImGui::PopStyleVar();
                ImGui::PopID();
                ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing()));
            }

            ImGui::Dummy(ImVec2(
                0.0f, glm::max(projectWinSize.y - ImGui::GetCursorPosY() - ImGui::GetStyle().WindowPadding.y, 0.0f)));

            ImGui::End();
        }

      private:
        void templateFile(std::string filePath, std::string projectName)
        {
            std::ifstream is{filePath};
            std::stringstream ss;
            ss << is.rdbuf();

            std::string str = ss.str();

            replaceAll(str, "{{PROJECT_NAME}}", projectName);
            replaceAll(str, "{{NETASSEMBLIES_PATH}}", (std::filesystem::current_path() / "NetAssemblies").string());

            std::ofstream s{filePath, std::ios::trunc};
            s << str;
            s.close();
        }

        void replaceAll(std::string& str, const std::string& from, const std::string& to)
        {
            size_t start_pos = 0;
            while ((start_pos = str.find(from, start_pos)) != std::string::npos)
            {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
            }
        }

        const EngineInterfaces& interfaces;
    };
}