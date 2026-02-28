#pragma once

namespace Window
{
    // Main window functions
    void CreateWindow(int Width, int Height, const char* Title);
    void DestroyWindow();
    void PollEvents();
    bool ShouldClose();

    // Extra functions
    void ImGuiInit();
    void ImGuiShutdown();

    void ClearColor(float r, float g, float b, float a);
    void SwapBuffers();
    bool IsKeyPressed(int key);
    

}