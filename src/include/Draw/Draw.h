#pragma once

#include "HandlerShared.h"

namespace Draw
{
void MainWindow();
void CleanUp();
void Render();

void DrawTopBar();
void DrawRackEditor(Rack &rack);
bool PopUpTool(Rack &rack);
void DrawModuleDetails();
void DrawScopeOverlay(const std::vector<float> *inputSamples,
					  const std::vector<float> *outputSamples,
					  const char *plotLabel);
                      
// Helper functions used across Draw translation units
Rack *FindRackByID(int rackID);
void DrawAvailableModulesChild(Rack &rack);
void Debug();
}