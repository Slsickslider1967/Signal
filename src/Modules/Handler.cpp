#include <iostream>
#include <list>
#include <thread>
#include <chrono>
#include "../../include/Functions/Window.h"
#include "../../include/Functions/Audio.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "imgui.h"
#include <vector>
#include <iostream>
#include <list>
#include <thread>
#include <chrono>
#include "../../include/Functions/Window.h"
#include "../../include/Functions/Audio.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "imgui.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>

// --Variables--
std::list<int> WaveFormGenList;
std::list<int> SpeedManipList;

enum TabType { TAB_WAVEFORM = 0, TAB_SPEED = 1 };
struct Tab {
    int id;
    TabType type;
    std::string title;
    bool open;
};

static std::vector<Tab> g_tabs;
static int g_nextTabId = 1;
static int g_activeTabId = 0;
static bool g_taskbarExpanded = false;

// --Forward declarations for addon modules--
namespace WaveFormGen
{
    void MainImgui();
}
namespace SpeedManipulation
{
    void MainImgui();
}

// --Function Prototypes--
void MainWindow();
void CleanUp();
void ImGuiFuncGen();
void ImGuiSpeedManip();
void Render();

int main()
{
    MainWindow();

    while (!Window::ShouldClose())
    {
        ImGuiUtil::Begin();
        ImGui::BeginMainMenuBar();

        if (ImGui::BeginMenu("Modules"))
        {
            if (ImGui::MenuItem("Waveform Generator"))
            {
                int id = g_nextTabId++;
                g_tabs.push_back(Tab{id, TAB_WAVEFORM, "Waveform", true});
                g_activeTabId = id;
            }
            if (ImGui::MenuItem("Speed Manipulator"))
            {
                int id = g_nextTabId++;
                g_tabs.push_back(Tab{id, TAB_SPEED, "Speed", true});
                g_activeTabId = id;
            }
            ImGui::EndMenu();
        }

        ImGui::NewLine();
        ImGui::Text("Signal Generator - ImGui Demo");

        ImGui::EndMainMenuBar();

        // Compact taskbar: shows the active module (single) and expands to show all open modules
        if (!g_tabs.empty())
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 0));
            ImGuiViewport* vp = ImGui::GetMainViewport();
            float bar_h = ImGui::GetFrameHeight();
            ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + bar_h));
            ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, bar_h));
            ImGuiWindowFlags flags_tb = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;
            if (ImGui::Begin("##ModuleTaskBar", NULL, flags_tb))
            {
                // find active label
                const char* activeLabel = "Modules";
                if (g_activeTabId != 0)
                {
                    for (const Tab &t : g_tabs)
                        if (t.id == g_activeTabId) { activeLabel = t.title.c_str(); break; }
                }

                if (!g_taskbarExpanded)
                {
                    if (ImGui::Button(activeLabel))
                        g_taskbarExpanded = true;
                }
                else
                {
                    // show all open modules as buttons with close small buttons
                    for (size_t i = 0; i < g_tabs.size(); ++i)
                    {
                        Tab &t = g_tabs[i];
                        char lbl[64];
                        std::snprintf(lbl, sizeof(lbl), "%s", t.title.c_str());
                        if (ImGui::Button(lbl))
                        {
                            g_activeTabId = t.id;
                            g_taskbarExpanded = false;
                        }
                        ImGui::SameLine();
                        char closeLbl[32];
                        std::snprintf(closeLbl, sizeof(closeLbl), "x##%d", t.id);
                        if (ImGui::SmallButton(closeLbl))
                        {
                            t.open = false;
                            if (g_activeTabId == t.id) g_activeTabId = 0;
                        }
                        if (i + 1 < g_tabs.size()) ImGui::SameLine();
                    }
                }
            }
            ImGui::End();
            ImGui::PopStyleVar();

            // Render tabs under the menu bar
            if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs))
            {
                for (size_t i = 0; i < g_tabs.size(); ++i)
                {
                    Tab &t = g_tabs[i];
                    char label[64];
                    std::snprintf(label, sizeof(label), "%s##%d", t.title.c_str(), t.id);
                    ImGui::PushID(t.id);
                    bool open = t.open;
                    ImGuiTabItemFlags itemFlags = (t.id == g_activeTabId) ? ImGuiTabItemFlags_SetSelected : 0;
                    if (ImGui::BeginTabItem(label, &open, itemFlags))
                    {
                        // mark active when opened
                        g_activeTabId = t.id;
                        // Render tab content depending on type
                        if (t.type == TAB_WAVEFORM)
                            WaveFormGen::MainImgui();
                        else if (t.type == TAB_SPEED)
                            SpeedManipulation::MainImgui();

                        ImGui::EndTabItem();
                    }
                    ImGui::PopID();
                    t.open = open;
                }
                ImGui::EndTabBar();
            }

            // Remove closed tabs
            g_tabs.erase(std::remove_if(g_tabs.begin(), g_tabs.end(), [](const Tab &t) { return !t.open; }), g_tabs.end());

            // ensure active tab id still valid
            if (g_activeTabId != 0)
            {
                bool found = false;
                for (const Tab &t : g_tabs) if (t.id == g_activeTabId) { found = true; break; }
                if (!found)
                    g_activeTabId = g_tabs.empty() ? 0 : g_tabs.front().id;
            }
        }

        ImGuiUtil::End();

        Render();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    CleanUp();
    return 0;
}

void MainWindow()
{
    setenv("PREFER_X11", "1", 1);

    Window::CreateWindow(1280, 720, "Signal Handler");
    Audio::Init();
    Window::PollEvents();
}

void CleanUp()
{
    Window::DestroyWindow();
    Audio::Close();
}

void Render()
{
    Window::ClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    ImGuiUtil::Render();
    Window::SwapBuffers();
    Window::PollEvents();
}

void ImGuiFuncGen()
{
    WaveFormGen::MainImgui();
}

void ImGuiSpeedManip()
{
    SpeedManipulation::MainImgui();
}
