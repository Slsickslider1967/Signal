#include <iostream>
#include "imgui.h"
#include "../../include/Functions/ImGuiUtil.h"

namespace SpeedManipulation
{
    // Render SpeedManipulator UI as an ImGui addon. Handler drives Begin/End.
    void MainImgui()
    {
        ImGui::Begin("Speed Manipulator");
        ImGui::Text("Speed manipulation controls go here.");
        // TODO: move any controls from the old standalone module into this function
        ImGui::End();
    }
}