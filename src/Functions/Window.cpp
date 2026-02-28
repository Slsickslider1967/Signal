#include <iostream>
#include "../../include/Functions/Window.h"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <thread>
#include <chrono>

namespace Window 
{
    static GLFWwindow* s_Window = nullptr;
    void CreateWindow(int Width, int Height, const char* Title)
    {
        std::cout << "Creating window: " << Title << " (" << Width << "x" << Height << ")" << std::endl;
        if (!glfwInit())
        {
            std::cerr << "Failed to initialize GLFW" << std::endl;
        }   
        s_Window = glfwCreateWindow(Width, Height, Title, NULL, NULL);
        if (!s_Window)
        {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return;
        }

        glfwMakeContextCurrent(s_Window);

        ImGuiInit();
        int should = glfwWindowShouldClose(s_Window);
        std::cout << "Window created successfully (initial shouldClose=" << should << ")" << std::endl;
    }

    void DestroyWindow()
    {
        std::cout << "Destroying window" << std::endl;
        if (s_Window)
        {
            // Shutdown ImGui properly before destroying the GL context
            ImGuiShutdown();
            glfwDestroyWindow(s_Window);
            s_Window = nullptr;
        }
        glfwTerminate();
        std::cout << "Window destroyed successfully" << std::endl;
    }

    void PollEvents()
    {
        glfwPollEvents();
    }

    bool ShouldClose()
    {
        if (!s_Window)
            return true;
        return glfwWindowShouldClose(s_Window);
    }

    void ImGuiInit()
    {
        std::cout << "Initializing ImGui" << std::endl;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        // Enable keyboard controls, docking and multi-viewport
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();
        // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows
        // look consistent with main window.
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Initialize platform/renderer backends
        ImGui_ImplGlfw_InitForOpenGL(s_Window, true);
        ImGui_ImplOpenGL3_Init("#version 330 core");

        std::cout << "ImGui initialized successfully" << std::endl;
    }

    // Properly shutdown ImGui and backends
    void ImGuiShutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ClearColor(float r, float g, float b, float a)
    {
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void SwapBuffers()
    {
        if (s_Window)
            glfwSwapBuffers(s_Window);
    }

    bool IsKeyPressed(int key)
    {
        if (!s_Window) return false;
        return glfwGetKey(s_Window, key) == GLFW_PRESS;
    }
}