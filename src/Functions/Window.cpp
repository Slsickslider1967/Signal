#include <iostream>
#include "../../include/Functions/Window.h"
#include <GLFW/glfw3.h>
#include "../../external/imgui/imgui.h"

namespace Window 
{
    void CreateWindow(int Width, int Height, const char* Title)
    {
        std::cout << "Creating window: " << Title << " (" << Width << "x" << Height << ")" << std::endl;
        if (!glfwInit())
        {
            std::cerr << "Failed to initialize GLFW" << std::endl;
        }   

        GLFWwindow* Window = glfwCreateWindow(Width, Height, Title, NULL, NULL);
        if (!Window)
        {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
        }

        ImGuiInit();

        glfwMakeContextCurrent(Window);
        std::cout << "Window created successfully" << std::endl;
    }

    void DestroyWindow()
    {
        std::cout << "Destroying window" << std::endl;
        glfwTerminate();
        std::cout << "Window destroyed successfully" << std::endl;
    }

    void PollEvents()
    {
        glfwPollEvents();
    }

    bool ShouldClose()
    {
        return glfwWindowShouldClose(glfwGetCurrentContext());
    }

    void ImGuiInit()
    {
        std::cout << "Initializing ImGui" << std::endl;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        std::cout << "ImGui initialized successfully" << std::endl;
    }
}