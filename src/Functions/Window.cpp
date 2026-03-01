#include <iostream>
#include "../../include/Functions/Window.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <thread>
#include <chrono>
#include <cstdlib>

namespace Window 
{
    static GLFWwindow* s_Window = nullptr;
    void CreateWindow(int Width, int Height, const char* Title)
    {
        // If the user requests X11 preference at runtime (for XWayland), adjust
        // environment so GLFW prefers X11 over Wayland before initialization.
        const char* preferX11 = std::getenv("PREFER_X11");
        if (preferX11 && preferX11[0] != '\0')
        {
            std::cout << "PREFER_X11 detected — preferring X11 (XWayland) backend" << std::endl;
            setenv("XDG_SESSION_TYPE", "x11", 1);
            unsetenv("WAYLAND_DISPLAY");
        }
        std::cout << "Creating window: " << Title << " (" << Width << "x" << Height << ")" << std::endl;
        // Optionally create the main GLFW window hidden so ImGui can use
        // platform (multi-)viewports as top-level windows while the "main"
        // host remains invisible. Enable by setting HIDE_MAIN_WINDOW=1.
        const char* hideMain = std::getenv("HIDE_MAIN_WINDOW");
        bool hideMainWindow = (hideMain && hideMain[0] != '\0');
        if (hideMainWindow)
        {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        }
        if (!glfwInit())
        {
            std::cerr << "Failed to initialize GLFW" << std::endl;
        }   
        // If hidden, creating a small offscreen window provides an OpenGL
        // context for ImGui platform windows to use.
        int winW = hideMainWindow ? 1 : Width;
        int winH = hideMainWindow ? 1 : Height;
        s_Window = glfwCreateWindow(winW, winH, Title, NULL, NULL);
        if (!s_Window)
        {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return;
        }

        glfwMakeContextCurrent(s_Window);
        // Initialize GL loader (glad) so ImGui multi-viewport/platform windows
        // can create and use GL functions in their contexts.
        int glad_ver = gladLoadGL(glfwGetProcAddress);
        if (glad_ver == 0)
        {
            std::cerr << "Failed to initialize GL loader (glad)" << std::endl;
        }

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
        // Report which display backend is active so the developer can verify
        // whether X11 (XWayland) or Wayland is being used by the process.
        const char* wayland = std::getenv("WAYLAND_DISPLAY");
        const char* display = std::getenv("DISPLAY");
        if (wayland && wayland[0] != '\0')
            std::cout << "Detected Wayland (WAYLAND_DISPLAY=" << wayland << ")" << std::endl;
        else if (display && display[0] != '\0')
            std::cout << "Detected X11 (DISPLAY=" << display << ")" << std::endl;
        else
            std::cout << "No Wayland or X11 display detected in environment" << std::endl;
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

    void GetFramebufferSize(int* width, int* height)
    {
        if (!s_Window)
        {
            if (width) *width = 0;
            if (height) *height = 0;
            return;
        }
        glfwGetFramebufferSize(s_Window, width, height);
    }
}