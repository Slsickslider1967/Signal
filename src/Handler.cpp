#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "imgui.h"
#include "imgui-knobs.h"
#include "imgui_stdlib.h"
#include "implot.h"
#include "imnodes.h"

#include "../include/Functions/Audio.h"
#include "../include/Functions/ImGuiUtil.h"
#include "../include/Functions/Module.h"
#include "../include/Functions/Window.h"
#include "../include/MDU/FileWatcher.h"
#include "../include/MDU/ModuleLoader.h"
#include "../include/Functions/ConsoleHandling.h"
#include "../include/MDU/CreateMDU.h"

struct Rack
{
    int ID;
    std::string Name;
    std::list<DynamicModule> DynamicModules;
    std::vector<Link> Links;
    bool Enabled = true;
};

static int NextRackID = 1;
static int NextModuleID = 1;
static int NextLinkID = 1;
static std::list<Rack> Racks;
static int SelectedModuleID = -1;
static int SelectedRackID = -1;

static MDU::ModuleLoader GModuleLoader;
static MDU::FileWatcher GFileWatcher;
static bool GFileWatcherInitialized = false;
static std::mutex GRackMutex;
static std::string GLastMduError;
static std::map<int, std::vector<float>> GModuleScopeInputs;
static std::map<int, std::vector<float>> GModuleScopeOutputs;
static bool GShowDebugConsole = false;

void MainWindow();
void CleanUp();
void Render();

void Debug();
void DrawRackEditor(Rack &rack);
void DrawTopBar();
void DrawLinks(Rack &rack);
void CreateLinks(Rack &rack);
void DrawModuleDetails();
void AddPins(const DynamicModule &module);

void SetupAudioHandling();
void ShutdownAudioHandling();
void UpdateAudioWaveFormsFromRacks();
void AudioFilterCallback(float *buffer, int numSamples, void *userData);

Rack *CreateRack(const std::string &name);
void DeleteRack(int rackID);
bool AddDynamicModuleToRack(Rack &rack, const std::string &sourcePath, std::string *errorOut);
void RemoveDynamicModuleFromRack(DynamicModule &module);
void RemoveNode(int nodeID);
bool PopUpTool(Rack &rack);
void DrawAvailableModulesChild(Rack &rack);
Rack *FindRackByID(int rackID);

std::vector<std::string> BuildMduRuntimePaths();
void RemoveLinksForModule(Rack &rack, int moduleID);
void RemoveDynamicModulesFromAllRacksBySourcePath(const std::string &sourcePath);
void ProcessMduFileChanges();

static constexpr int kScopeSampleCount = 512;

void CaptureScopeSamples(std::map<int, std::vector<float>> &scopeMap,
                         int moduleID,
                         const float *source,
                         int numSamples)
{
    std::vector<float> &scope = scopeMap[moduleID];
    if (scope.size() != static_cast<size_t>(kScopeSampleCount))
    {
        scope.assign(kScopeSampleCount, 0.0f);
    }

    if (source == nullptr || numSamples <= 0)
    {
        std::fill(scope.begin(), scope.end(), 0.0f);
        return;
    }

    int copyCount = std::min(kScopeSampleCount, numSamples);
    std::copy(source, source + copyCount, scope.begin());
    if (copyCount < kScopeSampleCount)
    {
        std::fill(scope.begin() + copyCount, scope.end(), 0.0f);
    }
}

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

std::vector<DynamicModule *> BuildProcessingOrder(Rack &rack)
{
    std::vector<DynamicModule *> ordered;
    std::map<int, DynamicModule *> moduleByID;
    std::set<int> activeModuleIDs;

    for (auto &module : rack.DynamicModules)
    {
        if (!module.Active || module.Instance == nullptr)
        {
            continue;
        }
        moduleByID[module.ID] = &module;
        activeModuleIDs.insert(module.ID);
    }

    std::set<int> processed;
    while (processed.size() < moduleByID.size())
    {
        bool progressed = false;

        for (auto &[moduleID, modulePtr] : moduleByID)
        {
            if (processed.find(moduleID) != processed.end())
            {
                continue;
            }

            bool ready = true;
            for (const auto &link : rack.Links)
            {
                if (link.EndModuleID != moduleID)
                {
                    continue;
                }

                if (activeModuleIDs.find(link.StartModuleID) == activeModuleIDs.end())
                {
                    continue;
                }

                if (processed.find(link.StartModuleID) == processed.end())
                {
                    ready = false;
                    break;
                }
            }

            if (ready)
            {
                ordered.push_back(modulePtr);
                processed.insert(moduleID);
                progressed = true;
            }
        }

        if (!progressed)
        {
            for (auto &[moduleID, modulePtr] : moduleByID)
            {
                if (processed.find(moduleID) == processed.end())
                {
                    ordered.push_back(modulePtr);
                    processed.insert(moduleID);
                }
            }
        }
    }

    return ordered;
}

const float *FindLinkedOutputBuffer(const Rack &rack,
                                    int endModuleID,
                                    int endPinIndex,
                                    const std::map<std::pair<int, int>, std::vector<float>> &outputBuffers)
{
    for (const auto &link : rack.Links)
    {
        if (link.EndModuleID != endModuleID || link.EndPinIndex != endPinIndex)
        {
            continue;
        }

        auto it = outputBuffers.find({link.StartModuleID, link.StartPinIndex});
        if (it == outputBuffers.end())
        {
            return nullptr;
        }

        if (it->second.empty())
        {
            return nullptr;
        }

        return it->second.data();
    }

    return nullptr;
}

int MakeInputAttributeID(int moduleID, int pinIndex)
{
    return moduleID * 1000 + 100 + pinIndex;
}

int MakeOutputAttributeID(int moduleID, int pinIndex)
{
    return moduleID * 1000 + pinIndex;
}

int main()
{
    MainWindow();

    while (!Window::ShouldClose())
    {
        ImGuiUtil::Begin();

        std::lock_guard<std::mutex> rackLock(GRackMutex);

        DrawTopBar();

        ImGuiViewport *mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos);
        ImGui::SetNextWindowSize(mainViewport->WorkSize);
        ImGui::SetNextWindowViewport(mainViewport->ID);

        ImGuiWindowFlags rackManagerFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::Begin("Rack Manager", nullptr, rackManagerFlags);

        for (auto rackIt = Racks.begin(); rackIt != Racks.end();)
        {
            Rack &rack = *rackIt;
            ImGui::PushID(rack.ID);

            std::string rackHeaderLabel = "Rack #" + std::to_string(rack.ID) + ": " + rack.Name + "###RackHeader" + std::to_string(rack.ID);
            bool rackOpen = ImGui::TreeNodeEx(rackHeaderLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::IsItemClicked())
            {
                SelectedRackID = rack.ID;
            }

            if (rackOpen)
            {
                DrawRackEditor(rack);
                if (PopUpTool(rack))
                {
                    ImGui::TreePop();
                    int rackIDToDelete = rack.ID;
                    auto nextRackIt = std::next(rackIt);
                    DeleteRack(rackIDToDelete);
                    rackIt = nextRackIt;
                    ImGui::PopID();
                    continue;
                }
                ImGui::TreePop();
            }

            ImGui::PopID();
            ++rackIt;
        }

        ImGui::End();

        if (SelectedModuleID != -1)
        {
            DrawModuleDetails();
        }

        ImGuiUtil::End();

        Render();

        UpdateAudioWaveFormsFromRacks();
        ProcessMduFileChanges();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    CleanUp();
    return 0;
}

// --Draw--

void DrawTopBar()
{

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Rack"))
        {
            if (ImGui::MenuItem("Add Rack"))
            {
                CreateRack(std::string("Rack ") + std::to_string(NextRackID));
            }
            if (ImGui::BeginMenu("Remove Rack"))
            {
                if (Racks.empty())
                {
                    ImGui::TextDisabled("No racks to remove");
                }
                else
                {
                    int rackIDToDelete = -1;
                    for (const auto &rack : Racks)
                    {
                        std::string label = "Rack #" + std::to_string(rack.ID) + ": " + rack.Name;
                        if (ImGui::MenuItem(label.c_str()))
                        {
                            rackIDToDelete = rack.ID;
                        }
                    }
                    if (rackIDToDelete != -1)
                    {
                        DeleteRack(rackIDToDelete);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Modules"))
        {
            if (ImGui::BeginMenu("Add Module to Selected Rack"))
            {
                Rack *selectedRack = FindRackByID(SelectedRackID);
                if (selectedRack == nullptr)
                {
                    ImGui::TextDisabled("Select a Rack First");
                }
                else
                {
                    DrawAvailableModulesChild(*selectedRack);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Create template MDU"))
            {
                MDU::CreateTemplateMDU();
            }
            if (ImGui::BeginMenu("Set MDU Search Paths"))
            {
                    std::string newPath;
                    if (ImGui::InputText("New Path", &newPath, ImGuiInputTextFlags_EnterReturnsTrue))
                    {                       
                        if (!newPath.empty())
                        {           
                            GModuleLoader.AddSearchPath(newPath);
                        }
                        else
                        {
                            Console::AppendConsoleLine("[warning] Cannot add empty path to MDU search paths.");
                        }
                    }
                    ImGui::Separator();
                    const auto &searchPaths = GModuleLoader.GetSearchPaths();
                    if (searchPaths.empty())
                    {
                        ImGui::TextDisabled("No search paths set");
                    }
                    else
                    {
                        for (const auto &path : searchPaths)
                        {
                            ImGui::Text("%s", path.c_str());
                        }
                    }
                    ImGui::EndMenu();
        
            }
            ImGui::EndMenu();

        }
        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Documentation"))
            {
                std::filesystem::path docPath = std::filesystem::current_path() / "docs" / "index.html";
                if (std::filesystem::exists(docPath))
                {
                    std::string command = "xdg-open " + docPath.string();
                    std::system(command.c_str());
                }
            }
            if (ImGui::MenuItem("GitHub Repository"))
            {
                std::string command = "xdg-open https://github.com/Slsickslider1967/Signal";
                std::system(command.c_str());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Console"))
            {
                GShowDebugConsole = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    Debug();
}

void Debug()
{
    if (!GShowDebugConsole)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(900, 420), ImGuiCond_FirstUseEver);
    ImGui::Begin("Debug Console", &GShowDebugConsole);

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

void MainWindow()
{
    setenv("PREFER_X11", "1", 1);
    Window::CreateWindow(12850, 720, "Signal Handler");
    Console::AppendConsoleLine("Window initialized: Signal Handler (12850x720)");
    SetupAudioHandling();
    Window::PollEvents();
}

void CleanUp()
{
    Window::DestroyWindow();
    ShutdownAudioHandling();
}

void Render()
{
    Window::ClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    ImGuiUtil::Render();
    Window::SwapBuffers();
    Window::PollEvents();
}

void DrawRackEditor(Rack &rack)
{
    ImNodes::BeginNodeEditor();
    if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        SelectedRackID = rack.ID;
        SelectedModuleID = -1;
    }

    bool openContextMenu = ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);

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

    if (openContextMenu)
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

void DrawLinks(Rack &rack)
{
    for (const auto &link : rack.Links)
    {
        ImNodes::Link(link.ID,
                      MakeOutputAttributeID(link.StartModuleID, link.StartPinIndex),
                      MakeInputAttributeID(link.EndModuleID, link.EndPinIndex));
    }
}

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

void DrawModuleDetails()
{
    DynamicModule *selectedModule = nullptr;
    for (auto &rack : Racks)
    {
        for (auto &module : rack.DynamicModules)
        {
            if (module.ID == SelectedModuleID)
            {
                selectedModule = &module;
                break;
            }
        }
        if (selectedModule != nullptr)
        {
            break;
        }
    }

    if (selectedModule == nullptr)
    {
        SelectedModuleID = -1;
        return;
    }

    bool windowOpen = true;
    ImGui::SetNextWindowSize(ImVec2(520.0f, 760.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, 460.0f), ImVec2(1400.0f, 1800.0f));
    std::string windowTitle = "Module Panel: " + selectedModule->Name + " #" + std::to_string(selectedModule->ID);
    ImGui::Begin(windowTitle.c_str(), &windowOpen);

    auto toLowerCopy = [](std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return text;
    };

    auto findParameter = [&](const std::string &parameterID) -> const MDU::ParameterDefinition *
    {
        for (const auto &parameter : selectedModule->Metadata.Parameters)
        {
            if (parameter.ID == parameterID)
            {
                return &parameter;
            }
        }
        return nullptr;
    };

    auto getParameter = [&](const std::string &parameterID, float fallback) -> float
    {
        float value = fallback;
        if (selectedModule->Instance != nullptr)
        {
            selectedModule->Instance->GetParameter(parameterID, value);
        }
        return value;
    };

    auto setParameterClamped = [&](const std::string &parameterID, float value)
    {
        const MDU::ParameterDefinition *parameter = findParameter(parameterID);
        if (parameter != nullptr)
        {
            if (value < parameter->minValue)
                value = parameter->minValue;
            if (value > parameter->maxValue)
                value = parameter->maxValue;
        }
        selectedModule->Instance->SetParameter(parameterID, value);
    };

    auto drawKnobByID = [&](const std::string &parameterID,
                            const char *title,
                            float fallback,
                            float minValue,
                            float maxValue,
                            float speed,
                            const char *format,
                            ImGuiKnobVariant variant,
                            ImGuiKnobFlags flags = ImGuiKnobFlags_NoTitle)
    {
        float value = getParameter(parameterID, fallback);
        ImGui::Text("%s", title);
        std::string knobID = "##" + parameterID + "_knob";
        if (ImGuiKnobs::Knob(knobID.c_str(), &value, minValue, maxValue, speed, format, variant, 0.0f, flags))
        {
            setParameterClamped(parameterID, value);
        }
    };

    auto drawSteppedButtons = [&](const std::string &parameterID, const char *sectionTitle)
    {
        const MDU::ParameterDefinition *parameter = findParameter(parameterID);
        if (parameter == nullptr || parameter->options.empty())
        {
            return;
        }

        int optionIndex = static_cast<int>(getParameter(parameterID, parameter->defaultValue));
        if (optionIndex < 0)
            optionIndex = 0;
        if (optionIndex >= static_cast<int>(parameter->options.size()))
            optionIndex = static_cast<int>(parameter->options.size()) - 1;

        ImGui::Text("%s", sectionTitle);
        for (int i = 0; i < static_cast<int>(parameter->options.size()); ++i)
        {
            bool selected = (i == optionIndex);
            if (selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
            }

            if (ImGui::Button(parameter->options[i].c_str(), ImVec2(86.0f, 0.0f)))
            {
                optionIndex = i;
                setParameterClamped(parameterID, static_cast<float>(optionIndex));
            }

            if (selected)
            {
                ImGui::PopStyleColor();
            }

            if (i + 1 < static_cast<int>(parameter->options.size()))
            {
                ImGui::SameLine();
            }
        }
    };

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.12f, 0.13f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));

    if (ImGui::BeginChild("ModuleFaceplate", ImVec2(0.0f, 0.0f), true))
    {
        ImGui::Text("%s", selectedModule->Name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", selectedModule->Metadata.ModuleType.c_str());

        ImGui::InputText("Module Name", &selectedModule->Name);
        ImGui::Checkbox("Module Active", &selectedModule->Active);
        ImGui::Separator();

        ImGui::Text("I/O");
        ImGui::Separator();
        ImGui::Text("Input Jacks: %d", selectedModule->InPins);
        ImGui::SameLine();
        ImGui::Text("Output Jacks: %d", selectedModule->OutPins);

        const std::string moduleTypeLower = toLowerCopy(selectedModule->Metadata.ModuleType);
        const std::string moduleNameLower = toLowerCopy(selectedModule->Metadata.ModuleName);

        if (selectedModule->Instance != nullptr)
        {
            if (moduleTypeLower.find("oscillator") != std::string::npos || moduleNameLower == "vco")
            {
                ImGui::Text("TUNING");
                if (ImGui::BeginTable("VCO_TUNING_ROW", 3, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    drawKnobByID("frequency", "FREQ", 440.0f, 20.0f, 2000.0f, 0.1f, "%.1f Hz", ImGuiKnobVariant_WiperDot, ImGuiKnobFlags_Logarithmic | ImGuiKnobFlags_NoTitle);

                    ImGui::TableNextColumn();
                    drawKnobByID("fm_depth", "FM DEPTH", 0.0f, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper);

                    ImGui::TableNextColumn();
                    drawKnobByID("amplitude", "OUTPUT LVL", 0.8f, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper);

                    ImGui::EndTable();
                }

                drawSteppedButtons("wave_type", "WAVEFORM");
            }
            else if (moduleNameLower == "lfo")
            {
                ImGui::Text("RATE");
                if (ImGui::BeginTable("LFO_RATE_ROW", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    drawKnobByID("rate_hz", "FREQ", 1.0f, 0.1f, 20.0f, 0.01f, "%.2f Hz", ImGuiKnobVariant_Wiper);

                    ImGui::TableNextColumn();
                    drawKnobByID("amount", "LEVEL", 0.5f, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper);
                    ImGui::EndTable();
                }

                drawSteppedButtons("wave_type", "WAVE SHAPE");
            }
            else if (moduleNameLower == "vcf" || moduleTypeLower.find("filter") != std::string::npos)
            {
                drawSteppedButtons("filter_type", "FILTER TYPE");
                ImGui::Text("CUTOFF / RESONANCE");
                if (ImGui::BeginTable("VCF_MAIN_ROW", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    drawKnobByID("cutoff_hz", "CUTOFF", 1000.0f, 20.0f, 20000.0f, 20.0f, "%.1f Hz", ImGuiKnobVariant_WiperDot, ImGuiKnobFlags_Logarithmic | ImGuiKnobFlags_NoTitle);

                    ImGui::TableNextColumn();
                    drawKnobByID("resonance", "RESONANCE", 0.5f, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper);
                    ImGui::EndTable();
                }
            }
            else if (moduleNameLower == "vca" || moduleTypeLower.find("amplifier") != std::string::npos)
            {
                ImGui::Text("AMPLIFIER");
                if (ImGui::BeginTable("VCA_AMP_ROW", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    drawKnobByID("base_gain", "GAIN", 0.8f, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper);

                    ImGui::TableNextColumn();
                    drawKnobByID("cv_amount", "CV AMT", 0.5f, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper);
                    ImGui::EndTable();
                }

                ImGui::Text("RANGE");
                if (ImGui::BeginTable("VCA_RANGE_ROW", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    drawKnobByID("range_min", "MIN", 0.0f, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper);

                    ImGui::TableNextColumn();
                    drawKnobByID("range_max", "MAX", 1.0f, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper);
                    ImGui::EndTable();
                }
            }
            else if (moduleNameLower == "output" || moduleTypeLower == "output")
            {
                ImGui::Text("MASTER OUTPUT");
                if (ImGui::BeginTable("OUT_ROW", 3, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    drawKnobByID("output_level", "LEVEL", 0.8f, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_WiperDot);
                    ImGui::TableNextColumn();
                    ImGui::EndTable();
                }

                float outputLevel = getParameter("output_level", 0.8f);
                if (outputLevel < 0.0f)
                    outputLevel = 0.0f;
                if (outputLevel > 1.0f)
                    outputLevel = 1.0f;
                ImGui::Text("Signal to DAC");
                ImGui::ProgressBar(outputLevel, ImVec2(-1.0f, 12.0f));
            }
            else
            {
                if (ImGui::BeginTable("FallbackParamTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
                {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

                    for (const auto &parameter : selectedModule->Metadata.Parameters)
                    {
                        float value = parameter.defaultValue;
                        selectedModule->Instance->GetParameter(parameter.ID, value);

                        ImGui::PushID(parameter.ID.c_str());
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted((parameter.label.empty() ? parameter.ID : parameter.label).c_str());
                        ImGui::TableSetColumnIndex(1);

                        if (parameter.type == MDU::ParameterType::Toggle)
                        {
                            bool boolValue = (value >= 0.5f);
                            if (ImGui::Checkbox("##toggle", &boolValue))
                            {
                                setParameterClamped(parameter.ID, boolValue ? 1.0f : 0.0f);
                            }
                        }
                        else if ((parameter.type == MDU::ParameterType::Combo || parameter.type == MDU::ParameterType::Stepped) && !parameter.options.empty())
                        {
                            int optionIndex = static_cast<int>(value);
                            if (optionIndex < 0)
                                optionIndex = 0;
                            if (optionIndex >= static_cast<int>(parameter.options.size()))
                                optionIndex = static_cast<int>(parameter.options.size()) - 1;

                            std::vector<const char *> items;
                            items.reserve(parameter.options.size());
                            for (const auto &option : parameter.options)
                            {
                                items.push_back(option.c_str());
                            }

                            if (ImGui::Combo("##combo", &optionIndex, items.data(), static_cast<int>(items.size())))
                            {
                                setParameterClamped(parameter.ID, static_cast<float>(optionIndex));
                            }
                        }
                        else
                        {
                            if (ImGui::SliderFloat("##slider", &value, parameter.minValue, parameter.maxValue, "%.3f"))
                            {
                                setParameterClamped(parameter.ID, value);
                            }
                        }

                        ImGui::PopID();
                    }

                    ImGui::EndTable();
                }
            }

            selectedModule->Instance->DrawEditor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Signal Scope");
            auto inputIt = GModuleScopeInputs.find(selectedModule->ID);
            auto outputIt = GModuleScopeOutputs.find(selectedModule->ID);

            if (inputIt == GModuleScopeInputs.end() || outputIt == GModuleScopeOutputs.end())
            {
                ImGui::TextDisabled("No scope data yet. Start audio and run the patch.");
            }
            else
            {
                DrawScopeOverlay(&inputIt->second, &outputIt->second, "Input vs Output");

                float peakOut = 0.0f;
                for (float sample : outputIt->second)
                {
                    float amplitude = sample < 0.0f ? -sample : sample;
                    if (amplitude > peakOut)
                    {
                        peakOut = amplitude;
                    }
                }
                if (peakOut > 1.0f)
                    peakOut = 1.0f;
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();

    bool requestRemove = ImGui::Button("Remove Module", ImVec2(-1.0f, 0.0f));
    ImGui::End();

    if (requestRemove)
    {
        Console::AppendConsoleLine("Remove Module clicked for module #" + std::to_string(selectedModule->ID) + " (" + selectedModule->Name + ")");
        RemoveNode(selectedModule->ID);
        SelectedModuleID = -1;
        return;
    }

    if (!windowOpen)
    {
        SelectedModuleID = -1;
    }
}

// --Audio Handling--

void SetupAudioHandling()
{
    Audio::Init();
    Audio::SetFilterCallback(AudioFilterCallback, nullptr);
}

void UpdateAudioWaveFormsFromRacks()
{
    // Keep legacy oscillator list empty so only the MDU callback writes audio.
    static bool initialized = false;
    if (!initialized)
    {
        std::vector<WaveForm> empty;
        Audio::SetWaveForms(empty);
        initialized = true;
    }
}

void AudioFilterCallback(float *buffer, int numSamples, void *userData)
{
    (void)userData;

    std::lock_guard<std::mutex> rackLock(GRackMutex);

    if (buffer == nullptr || numSamples <= 0)
    {
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        buffer[i] = 0.0f;
    }

    for (auto &rack : Racks)
    {
        if (!rack.Enabled || rack.DynamicModules.empty())
        {
            continue;
        }

        std::map<std::pair<int, int>, std::vector<float>> outputBuffers;
        std::vector<DynamicModule *> processingOrder = BuildProcessingOrder(rack);

        for (DynamicModule *module : processingOrder)
        {
            if (module == nullptr || !module->Active || module->Instance == nullptr)
            {
                continue;
            }

            std::vector<const float *> inputPins(module->InPins, nullptr);
            for (int pinIndex = 0; pinIndex < module->InPins; ++pinIndex)
            {
                inputPins[pinIndex] = FindLinkedOutputBuffer(rack, module->ID, pinIndex, outputBuffers);
            }

            std::vector<float *> outputPins(module->OutPins, nullptr);
            for (int pinIndex = 0; pinIndex < module->OutPins; ++pinIndex)
            {
                auto &pinBuffer = outputBuffers[{module->ID, pinIndex}];
                pinBuffer.assign(numSamples, 0.0f);
                outputPins[pinIndex] = pinBuffer.data();
            }

            const float *firstInput = inputPins.empty() ? nullptr : inputPins[0];
            CaptureScopeSamples(GModuleScopeInputs, module->ID, firstInput, numSamples);

            MDU::BufferView bufferView;
            bufferView.InputPins.assign(inputPins.begin(), inputPins.end());
            bufferView.OutputPins.assign(outputPins.begin(), outputPins.end());
            bufferView.NumberOfSamples = static_cast<size_t>(numSamples);
            bufferView.SampleRate = 44100;

            module->Instance->Process(bufferView, 0.0f);

            const float *firstOutput = outputPins.empty() ? nullptr : outputPins[0];
            CaptureScopeSamples(GModuleScopeOutputs, module->ID, firstOutput, numSamples);
        }

        for (const auto &module : rack.DynamicModules)
        {
            if (!module.Active)
            {
                continue;
            }

            bool isOutputModule = (module.Metadata.ModuleType == "Output" || module.Metadata.ModuleName == "Output");
            if (!isOutputModule)
            {
                continue;
            }

            auto outIt = outputBuffers.find({module.ID, 0});
            if (outIt == outputBuffers.end())
            {
                continue;
            }

            const auto &outBuffer = outIt->second;
            for (int i = 0; i < numSamples && i < static_cast<int>(outBuffer.size()); ++i)
            {
                float mixed = buffer[i] + outBuffer[i];
                if (mixed > 1.0f)
                    mixed = 1.0f;
                if (mixed < -1.0f)
                    mixed = -1.0f;
                buffer[i] = mixed;
            }
        }
    }
}

void ShutdownAudioHandling()
{
    Audio::Close();
}

// --Tools--

Rack *CreateRack(const std::string &name)
{
    Rack newRack;
    newRack.ID = NextRackID++;
    newRack.Name = name;
    newRack.Enabled = true;
    Racks.push_back(newRack);
    Console::AppendConsoleLine("Rack created: '" + newRack.Name + "' (ID #" + std::to_string(newRack.ID) + ")");
    return &Racks.back();
}

void DeleteRack(int rackID)
{
    for (auto it = Racks.begin(); it != Racks.end(); ++it)
    {
        if (it->ID != rackID)
        {
            continue;
        }

        for (auto &module : it->DynamicModules)
        {
            RemoveDynamicModuleFromRack(module);
        }

        Racks.erase(it);
        return;
    }
}

bool AddDynamicModuleToRack(Rack &rack, const std::string &sourcePath, std::string *errorOut)
{
    const auto &loadedMap = GModuleLoader.GetLoadedModules();
    auto it = loadedMap.find(sourcePath);
    if (it == loadedMap.end())
    {
        const std::string message = "Loaded module not found for path: " + sourcePath;
        if (errorOut)
        {
            *errorOut = message;
        }
        Console::AppendConsoleLine("[error] " + message);
        return false;
    }

    const auto &loaded = it->second;
    if (loaded.Create == nullptr || loaded.Destroy == nullptr)
    {
        const std::string message = "Factory functions missing for: " + sourcePath;
        if (errorOut)
        {
            *errorOut = message;
        }
        Console::AppendConsoleLine("[error] " + message);
        return false;
    }

    MDU::Module *instance = loaded.Create();
    if (instance == nullptr)
    {
        const std::string message = "Create returned null for: " + sourcePath;
        if (errorOut)
        {
            *errorOut = message;
        }
        Console::AppendConsoleLine("[error] " + message);
        return false;
    }

    DynamicModule dynamicModule;
    dynamicModule.ID = NextModuleID++;
    dynamicModule.SourcePath = sourcePath;
    dynamicModule.Metadata = loaded.Metadata;
    dynamicModule.Name = loaded.Metadata.ModuleName.empty() ? "MDU Module" : loaded.Metadata.ModuleName;
    dynamicModule.Instance = instance;
    dynamicModule.Destroy = loaded.Destroy;
    dynamicModule.InPins = static_cast<int>(loaded.Metadata.InputPins.size());
    dynamicModule.OutPins = static_cast<int>(loaded.Metadata.OutputPins.size());

    rack.DynamicModules.push_back(dynamicModule);
    Console::AppendConsoleLine("Module '" + dynamicModule.Name + "' added to rack '" + rack.Name + "' (Rack #" + std::to_string(rack.ID) + ")");
    return true;
}

void RemoveDynamicModuleFromRack(DynamicModule &module)
{
    if (module.Instance != nullptr && module.Destroy != nullptr)
    {
        module.Destroy(module.Instance);
    }
    module.Instance = nullptr;
    module.Destroy = nullptr;
}

void RemoveNode(int nodeID)
{
    for (auto &rack : Racks)
    {
        rack.DynamicModules.remove_if([nodeID](DynamicModule &module)
                                      {
                                          if (module.ID != nodeID)
                                          {
                                              return false;
                                          }

                                          RemoveDynamicModuleFromRack(module);
                                          return true;
                                      });

        rack.Links.erase(
            std::remove_if(rack.Links.begin(), rack.Links.end(),
                           [nodeID](const Link &link)
                           { return link.StartModuleID == nodeID || link.EndModuleID == nodeID; }),
            rack.Links.end());
    }
}

bool PopUpTool(Rack &rack)
{
    bool requestDelete = false;
    if (ImGui::BeginPopupModal("RackContextMenu"))
    {
        if (ImGui::InputText("Rack Name", &rack.Name, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::Separator();
        ImGui::Text("Available Modules");
        DrawAvailableModulesChild(rack);

        ImGui::Separator();
        if (ImGui::Selectable("Remove Rack"))
        {
            requestDelete = true;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    return requestDelete;
}

void DrawAvailableModulesChild(Rack &rack)
{
    if (!ImGui::BeginChild("AvailableModulesChild", ImVec2(0.0f, 220.0f), true))
    {
        ImGui::EndChild();
        return;
    }

    const auto &loadedModules = GModuleLoader.GetLoadedModules();
    ImGui::Text("Loaded: %d", static_cast<int>(loadedModules.size()));
    if (loadedModules.empty())
    {
        ImGui::TextDisabled("No loaded .mdu modules");
        if (!GLastMduError.empty())
        {
            ImGui::Separator();
            ImGui::TextWrapped("Last loader error: %s", GLastMduError.c_str());
        }
    }
    else
    {
        for (const auto &entry : loadedModules)
        {
            const std::string &sourcePath = entry.first;
            const auto &loadedModule = entry.second;

            std::string label = loadedModule.Metadata.ModuleName.empty() ? sourcePath : loadedModule.Metadata.ModuleName;

            if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_DontClosePopups))
            {
                std::string error;
                AddDynamicModuleToRack(rack, sourcePath, &error);
                if (!error.empty())
                {
                    std::cerr << error << std::endl;
                }
            }
        }
    }

    ImGui::EndChild();
}

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

// --mdu handling--

std::vector<std::string> BuildMduRuntimePaths()
{
    std::vector<std::string> candidates = {
        "src/Modules",
        "modules",
        "../src/Modules",
        "../modules"};

    std::vector<std::string> validPaths;
    validPaths.reserve(candidates.size());

    for (const auto &candidate : candidates)
    {
        std::error_code errorCode;
        std::filesystem::path absolutePath = std::filesystem::absolute(candidate, errorCode);
        if (errorCode)
        {
            continue;
        }

        if (std::filesystem::exists(absolutePath, errorCode) && !errorCode)
        {
            validPaths.push_back(absolutePath.lexically_normal().string());
        }
    }

    if (validPaths.empty())
    {
        validPaths.push_back("src/Modules");
        validPaths.push_back("modules");
    }

    return validPaths;
}

void RemoveLinksForModule(Rack &rack, int moduleID)
{
    rack.Links.erase(
        std::remove_if(rack.Links.begin(), rack.Links.end(),
                       [moduleID](const Link &link)
                       { return link.StartModuleID == moduleID || link.EndModuleID == moduleID; }),
        rack.Links.end());
}

void RemoveDynamicModulesFromAllRacksBySourcePath(const std::string &sourcePath)
{
    for (auto &rack : Racks)
    {
        rack.DynamicModules.remove_if([&rack, &sourcePath](DynamicModule &module)
                                      {
                                          if (module.SourcePath != sourcePath)
                                          {
                                              return false;
                                          }

                                          int removedModuleID = module.ID;
                                          RemoveDynamicModuleFromRack(module);
                                          RemoveLinksForModule(rack, removedModuleID);
                                          return true;
                                      });
    }
}

void ProcessMduFileChanges()
{
    if (!GFileWatcherInitialized)
    {
        const auto runtimePaths = BuildMduRuntimePaths();
        GFileWatcher.SetWatchPaths(runtimePaths);
        GModuleLoader.SetSearchPaths(runtimePaths);

        std::string loadAllError;
        if (!GModuleLoader.ScanAndLoadAll(&loadAllError) && !loadAllError.empty())
        {
            GLastMduError = loadAllError;
            std::cerr << loadAllError << std::endl;
            Console::AppendConsoleLine("[error] " + loadAllError);
        }
        else
        {
            GLastMduError.clear();
        }

        GFileWatcher.PrimeSnapshot();
        GFileWatcherInitialized = true;
    }

    for (const auto &change : GFileWatcher.PollChanges())
    {
        if (change.Type == MDU::FileChangeType::Added || change.Type == MDU::FileChangeType::Modified)
        {
            if (change.Type == MDU::FileChangeType::Modified)
            {
                RemoveDynamicModulesFromAllRacksBySourcePath(change.Path);
            }

            std::string error;
            GModuleLoader.LoadFromMduFile(change.Path, &error);
            if (!error.empty())
            {
                GLastMduError = error;
                std::cerr << error << std::endl;
                Console::AppendConsoleLine("[error] " + error + " (loading " + change.Path + ")");
            }
            else
            {
                GLastMduError.clear();
            }
        }
        else if (change.Type == MDU::FileChangeType::Removed)
        {
            RemoveDynamicModulesFromAllRacksBySourcePath(change.Path);

            std::string error;
            GModuleLoader.UnloadByPath(change.Path, &error);
            if (!error.empty())
            {
                GLastMduError = error;
                std::cerr << error << std::endl;
                Console::AppendConsoleLine("[error] " + error + " (unloading " + change.Path + ")");
            }
            else
            {
                GLastMduError.clear();
            }
        }
    }
}
