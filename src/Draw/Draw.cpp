#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imnodes.h"
#include "implot.h"

#include "Audio/Record.h"
#include "Draw/Draw.h"
#include "Draw/ImGuiUtil.h"
#include "Draw/Window.h"
#include "Functions/ConsoleHandling.h"
#include "MDU/CreateMDU.h"

namespace Draw
{
    // Draw overlaid input/output scope traces for a module panel.
    void DrawScopeOverlay(const std::vector<float> *inputSamples,
                          const std::vector<float> *outputSamples,
                          const char *plotLabel)
    {
        if (inputSamples == nullptr || outputSamples == nullptr)
        {
            return;
        }

        if (inputSamples->empty() || outputSamples->empty())
        {
            return;
        }

        ImPlotFlags plotFlags = ImPlotFlags_NoMenus |
                                ImPlotFlags_NoBoxSelect |
                                ImPlotFlags_NoMouseText;
        ImPlotAxisFlags axisFlags = ImPlotAxisFlags_NoDecorations |
                                    ImPlotAxisFlags_Lock;

        if (ImPlot::BeginPlot(plotLabel, ImVec2(-1.0f, 170.0f), plotFlags))
        {
            int sampleCount = std::min(static_cast<int>(inputSamples->size()), static_cast<int>(outputSamples->size()));
            ImPlot::SetupAxes(nullptr, nullptr, axisFlags, axisFlags);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, static_cast<double>(sampleCount), ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1.1, 1.1, ImGuiCond_Always);

            ImPlot::PlotLine("Input", inputSamples->data(), sampleCount);
            ImPlot::PlotLine("Output", outputSamples->data(), sampleCount);

            ImPlot::EndPlot();
        }
    }

    // Build a unique imnodes attribute ID for an input pin.
    int MakeInputAttributeID(int moduleID, int pinIndex)
    {
        return moduleID * 1000 + 100 + pinIndex;
    }

    // Build a unique imnodes attribute ID for an output pin.
    int MakeOutputAttributeID(int moduleID, int pinIndex)
    {
        return moduleID * 1000 + pinIndex;
    }

    // Find a rack by runtime ID and return a mutable pointer.
    Rack *FindRackByID(int rackID)
    {
        for (auto &rack : Racks)
        {
            if (rack.ID == rackID)
            {
                return &rack;
            }
        }
        return nullptr;
    }



    // Render all existing links for the active node editor.
    void DrawLinks(Rack &rack)
    {
        for (const auto &link : rack.Links)
        {
            ImNodes::Link(link.ID,
                          MakeOutputAttributeID(link.StartModuleID, link.StartPinIndex),
                          MakeInputAttributeID(link.EndModuleID, link.EndPinIndex));
        }
    }

    // Create a new link when a valid output->input connection is made in the editor.
    void CreateLinks(Rack &rack)
    {
        int startAttr = 0;
        int endAttr = 0;
        if (ImNodes::IsLinkCreated(&startAttr, &endAttr))
        {
            int startModuleID = startAttr / 1000;
            int endModuleID = endAttr / 1000;
            int startPin = startAttr % 1000;
            int endPin = endAttr % 1000;

            bool isStartOutput = (startPin < 100);
            bool isEndInput = (endPin >= 100);
            if (!isStartOutput || !isEndInput)
            {
                return;
            }

            bool linkExists = false;
            for (const auto &link : rack.Links)
            {
                if (link.StartModuleID == startModuleID && link.EndModuleID == endModuleID)
                {
                    linkExists = true;
                    break;
                }
            }

            if (!linkExists)
            {
                Link newLink;
                newLink.ID = NextLinkID++;
                newLink.StartModuleID = startModuleID;
                newLink.EndModuleID = endModuleID;
                newLink.StartPinIndex = startPin;
                newLink.EndPinIndex = endPin - 100;
                rack.Links.push_back(newLink);
            }
        }
    }

    // Draw named input/output pin attributes for one module node.
    void AddPins(const DynamicModule &module)
    {
        for (int i = 0; i < module.InPins; ++i)
        {
            std::string label = "In";
            if (i < static_cast<int>(module.Metadata.InputPins.size()))
            {
                const auto &pin = module.Metadata.InputPins[i];
                if (!pin.label.empty())
                {
                    label = pin.label;
                }
                else if (!pin.ID.empty())
                {
                    label = pin.ID;
                }
            }

            ImNodes::BeginInputAttribute(MakeInputAttributeID(module.ID, i));
            ImGui::Text("%s", label.c_str());
            ImNodes::EndInputAttribute();
        }

        for (int i = 0; i < module.OutPins; ++i)
        {
            std::string label = "Out";
            if (i < static_cast<int>(module.Metadata.OutputPins.size()))
            {
                const auto &pin = module.Metadata.OutputPins[i];
                if (!pin.label.empty())
                {
                    label = pin.label;
                }
                else if (!pin.ID.empty())
                {
                    label = pin.ID;
                }
            }

            ImNodes::BeginOutputAttribute(MakeOutputAttributeID(module.ID, i));
            ImGui::Text("%s", label.c_str());
            ImNodes::EndOutputAttribute();
        }
    }

    // Render the detachable debug console window.
    void Debug()
    {
        if (!GlobalShowDebugConsole)
        {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(900, 420), ImGuiCond_FirstUseEver);
        ImGui::Begin("Debug Console", &GlobalShowDebugConsole);

        ImGui::SameLine();

        ImGui::SameLine();
        if (ImGui::Button("Clear Output"))
        {
            Console::ClearConsoleOutput();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", Console::AutoScrollFlag());

        ImGui::Separator();

        if (ImGui::BeginChild("ConsoleOutput", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar))
        {
            std::vector<std::string> lines = Console::GetConsoleLinesSnapshot();
            for (const std::string &line : lines)
            {
                ImGui::TextUnformatted(line.c_str());
            }

            if (*Console::AutoScrollFlag() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
            {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }
}

namespace Draw
{
    // Initialize the app window and audio pipeline for the editor session.
    void MainWindow()
    {
        #if (defined(__linux__) || defined(__unix__) || defined(__APPLE__))
                setenv("PREFER_X11", "1", 1);
        #endif
        int Width = 1020;
        int Height = 720;
        Window::CreateWindow(Width, Height, "Signal Handler");
        Console::AppendConsoleLine("Window initialized: Signal Handler (" + std::to_string(Width) + "x" + std::to_string(Height) + ")");
        SetupAudioHandling();
        Window::PollEvents();
    }

    // Tear down rendering window and audio resources.
    void CleanUp()
    {
        Window::DestroyWindow();
        ShutdownAudioHandling();
    }

    // Render one frame and present it.
    void Render()
    {
        Window::ClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        ImGuiUtil::Render();
        Window::SwapBuffers();
        Window::PollEvents();
    }


    // Draw the node editor for one rack, including modules, links, and context actions.
    void DrawRackEditor(Rack &rack)
    {
        ImNodes::BeginNodeEditor();
        if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (std::find(SelectedRackIDs.begin(), SelectedRackIDs.end(), rack.ID) == SelectedRackIDs.end())
            {
                SelectedRackIDs.push_back(rack.ID);
            }
            SelectedModuleID = -1;
        }

        bool openContextMenu = ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);

        // Draw each module node with pins and metadata.
        for (auto moduleIt = rack.DynamicModules.begin(); moduleIt != rack.DynamicModules.end(); ++moduleIt)
        {
            DynamicModule &module = *moduleIt;

            ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
            ImNodes::BeginNode(module.ID);

            AddPins(module);

            ImGui::Text("MDU: %s", module.Name.c_str());
            if (!module.Metadata.Author.empty())
            {
                ImGui::TextDisabled("by %s", module.Metadata.Author.c_str());
            }

            ImNodes::EndNode();
            ImNodes::PopAttributeFlag();

            if (ImNodes::IsNodeSelected(module.ID))
            {
                SelectedModuleID = module.ID;
            }
        }

        DrawLinks(rack);
        ImNodes::EndNodeEditor();

        if (openContextMenu && !ShowModuleDetails && SelectedModuleID == -1)
        {
            ImGui::OpenPopup("RackContextMenu");
        }

        CreateLinks(rack);

        int deletedLinkID = 0;
        if (ImNodes::IsLinkDestroyed(&deletedLinkID))
        {
            rack.Links.erase(
                std::remove_if(rack.Links.begin(), rack.Links.end(),
                               [deletedLinkID](const Link &link)
                               { return link.ID == deletedLinkID; }),
                rack.Links.end());
        }
    }
}