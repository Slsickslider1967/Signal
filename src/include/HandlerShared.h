#pragma once

#include <filesystem>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "Functions/Module.h"
#include "MDU/ModuleLoader.h"

struct Rack
{
    int ID;
    std::string Name;
    std::list<DynamicModule> DynamicModules;
    std::vector<Link> Links;
    bool Enabled = true;
};

extern int NextRackID;
extern int NextModuleID;
extern int NextLinkID;
extern int SelectedModuleID;
extern int SelectedRackID;

extern std::list<Rack> Racks;

extern MDU::ModuleLoader GModuleLoader;
extern std::mutex GRackMutex;

extern std::string GLastMduError;
extern std::map<int, std::vector<float>> GModuleScopeInputs;
extern std::map<int, std::vector<float>> GModuleScopeOutputs;

extern bool GShowDebugConsole;
extern bool IsRecording;

Rack *CreateRack(const std::string &name);
void DeleteRack(int rackID);
bool AddDynamicModuleToRack(Rack &rack, const std::string &sourcePath, std::string *errorOut);
void RemoveNode(int nodeID);
void LaunchDefaultFileManager(const std::filesystem::path &path = {});

void SetupAudioHandling();
void ShutdownAudioHandling();
