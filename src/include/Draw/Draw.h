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
}