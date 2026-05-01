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

extern std::list<int> SelectedRackIDs;
extern std::list<int> SelectedModuleIDs;
extern std::list<Rack> Racks;


extern MDU::ModuleLoader GlobalModuleLoader;
extern std::mutex GlobalRackMutex;

extern std::string GlobalLastMduError;
extern std::map<int, std::vector<float>> GlobaloduleScopeInputs;
extern std::map<int, std::vector<float>> GlobalModuleScopeOutputs;

extern bool GlobalShowDebugConsole;
extern bool IsRecording;
extern bool ShowModuleDetails;

Rack *CreateRack(const std::string &name);
void DeleteRack(int rackID);
bool AddDynamicModuleToRack(Rack &rack, const std::string &sourcePath, std::string *errorOut);
void RemoveNode(int nodeID);
void LaunchDefaultFileManager(const std::filesystem::path &path = {});
void AddMduSearchPathAndPersist(const std::string &path);

void SetupAudioHandling();
void ShutdownAudioHandling();
