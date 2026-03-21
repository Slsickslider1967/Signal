#include <iostream>
#include "../../include/Functions/Window.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <string>
#include "imgui.h"
#include "implot.h"
#include "imnodes.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "../../include/Functions/ConsoleHandling.h"

#include <thread>
#include <chrono>
#include <cstdlib>

namespace Window 
{
    // --Function Prototypes--
    void ImGuiInit();
    void ImGuiShutdown();
    void ImGuiStyleSetUp(ImGuiStyle &style, ImVec4* colours);

    static GLFWwindow* s_Window = nullptr;
    void CreateWindow(int Width, int Height, const char* Title)
    {
        //X11 for multi viewports
        const char* preferX11 = std::getenv("PREFER_X11");
        if (preferX11 && preferX11[0] != '\0')
        {
            std::string Message = "PREFER_X11 detected — preferring X11 (XWayland) backend";
            std::cout << Message << std::endl;
            Console::AppendConsoleLine(Message);

            setenv("XDG_SESSION_TYPE", "x11", 1);
            unsetenv("WAYLAND_DISPLAY");
        }
        std::cout << "Creating window: " << Title << " (" << Width << "x" << Height << ")" << std::endl;
        Console::AppendConsoleLine("Creating window: " + std::string(Title) + " (" + std::to_string(Width) + "x" + std::to_string(Height) + ")");
        const char* hideMain = std::getenv("HIDE_MAIN_WINDOW");
        bool hideMainWindow = (hideMain && hideMain[0] != '\0');
        if (hideMainWindow)
        {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        }
        if (!glfwInit())
        {
            std::string Message = "Failed to initialize GLFW";
            std::cerr << Message << std::endl;
            Console::AppendConsoleLine(Message);
        }   

        int winW = hideMainWindow ? 1 : Width;
        int winH = hideMainWindow ? 1 : Height;
        s_Window = glfwCreateWindow(winW, winH, Title, NULL, NULL);
        if (!s_Window)
        {
            std::string Message = "Failed to create GLFW window";
            std::cerr << Message << std::endl;
            Console::AppendConsoleLine(Message);
            glfwTerminate();
            return;
        }

        glfwMakeContextCurrent(s_Window);
        int glad_ver = gladLoadGL(glfwGetProcAddress);
        if (glad_ver == 0)
        {
            std::string Message = "Failed to initialize GL loader (glad)";
            Console::AppendConsoleLine(Message);
            std::cerr << "Failed to initialize GL loader (glad)" << std::endl;
        }

        ImGuiInit();
        int should = glfwWindowShouldClose(s_Window);
        std::string Message = "Window created successfully (initial shouldClose=" + std::to_string(should) + ")";
        Console::AppendConsoleLine(Message);
        std::cout << "Window created successfully (initial shouldClose=" << should << ")" << std::endl;
    }

    void DestroyWindow()
    {
        std::string Message = "Destroying window";
        Console::AppendConsoleLine(Message);
        if (s_Window)
        {
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
        Console::AppendConsoleLine("Initializing ImGui");

        const char* wayland = std::getenv("WAYLAND_DISPLAY");
        const char* display = std::getenv("DISPLAY");
        if (wayland && wayland[0] != '\0')
        {
            std::string Message = "Detected Wayland (WAYLAND_DISPLAY=" + std::string(wayland) + ")";
            std::cout << Message << std::endl;
            Console::AppendConsoleLine(Message);
        }
        else if (display && display[0] != '\0')
        {
            std::string Message = "Detected X11 (DISPLAY=" + std::string(display) + ")";
            std::cout << Message << std::endl;
            Console::AppendConsoleLine(Message);
        }
        else
        {
            std::string Message = "No Wayland or X11 display detected in environment";
            std::cout << Message << std::endl;
            Console::AppendConsoleLine(Message);
        }
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();  
        ImNodes::CreateContext();        
        ImNodes::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colours = style.Colors;
        
        ImGuiStyleSetUp(style, colours);
        
        // Multi-viewport compatibility
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 6.0f;
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
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        ImNodes::DestroyContext();

        std::cout << "ImGui shutdown successfully" << std::endl;
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

    void ImGuiStyleSetUp(ImGuiStyle &style, ImVec4* colours)
    {
        colours[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
        colours[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colours[ImGuiCol_WindowBg]               = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        colours[ImGuiCol_ChildBg]                = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
        colours[ImGuiCol_PopupBg]                = ImVec4(0.10f, 0.10f, 0.12f, 0.95f);
        colours[ImGuiCol_Border]                 = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
        colours[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colours[ImGuiCol_FrameBg]                = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
        colours[ImGuiCol_FrameBgHovered]         = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
        colours[ImGuiCol_FrameBgActive]          = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);
        colours[ImGuiCol_TitleBg]                = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colours[ImGuiCol_TitleBgActive]          = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
        colours[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.10f, 0.10f, 0.12f, 0.75f);
        colours[ImGuiCol_MenuBarBg]              = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
        colours[ImGuiCol_ScrollbarBg]            = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colours[ImGuiCol_ScrollbarGrab]          = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);
        colours[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.44f, 1.00f);
        colours[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.50f, 0.50f, 0.54f, 1.00f);
        colours[ImGuiCol_CheckMark]              = ImVec4(0.20f, 0.70f, 0.90f, 1.00f);
        colours[ImGuiCol_SliderGrab]             = ImVec4(0.20f, 0.70f, 0.90f, 1.00f);
        colours[ImGuiCol_SliderGrabActive]       = ImVec4(0.30f, 0.80f, 1.00f, 1.00f);
        colours[ImGuiCol_Button]                 = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
        colours[ImGuiCol_ButtonHovered]          = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
        colours[ImGuiCol_ButtonActive]           = ImVec4(0.20f, 0.70f, 0.90f, 1.00f);
        colours[ImGuiCol_Header]                 = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
        colours[ImGuiCol_HeaderHovered]          = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
        colours[ImGuiCol_HeaderActive]           = ImVec4(0.25f, 0.65f, 0.85f, 1.00f);
        colours[ImGuiCol_Separator]              = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
        colours[ImGuiCol_SeparatorHovered]       = ImVec4(0.35f, 0.35f, 0.39f, 1.00f);
        colours[ImGuiCol_SeparatorActive]        = ImVec4(0.40f, 0.40f, 0.44f, 1.00f);
        colours[ImGuiCol_ResizeGrip]             = ImVec4(0.20f, 0.20f, 0.23f, 1.00f);
        colours[ImGuiCol_ResizeGripHovered]      = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
        colours[ImGuiCol_ResizeGripActive]       = ImVec4(0.20f, 0.70f, 0.90f, 1.00f);
        colours[ImGuiCol_Tab]                    = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
        colours[ImGuiCol_TabHovered]             = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
        colours[ImGuiCol_TabActive]              = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
        colours[ImGuiCol_TabUnfocused]           = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
        colours[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.20f, 0.20f, 0.23f, 1.00f);
        colours[ImGuiCol_DockingPreview]         = ImVec4(0.20f, 0.70f, 0.90f, 0.70f);
        colours[ImGuiCol_DockingEmptyBg]         = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
        colours[ImGuiCol_PlotLines]              = ImVec4(0.20f, 0.70f, 0.90f, 1.00f);
        colours[ImGuiCol_PlotLinesHovered]       = ImVec4(0.30f, 0.80f, 1.00f, 1.00f);
        colours[ImGuiCol_PlotHistogram]          = ImVec4(0.20f, 0.70f, 0.90f, 1.00f);
        colours[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.30f, 0.80f, 1.00f, 1.00f);
        colours[ImGuiCol_TableHeaderBg]          = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
        colours[ImGuiCol_TableBorderStrong]      = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
        colours[ImGuiCol_TableBorderLight]       = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
        colours[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colours[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
        colours[ImGuiCol_TextSelectedBg]         = ImVec4(0.25f, 1.55f, 1.95f, 1.f);
        colours[ImGuiCol_DragDropTarget]         = ImVec4(1.f, .75f, .3333333333333333F, .9F);
        colours[ImGuiCol_NavHighlight]           = ImVec4(0.20f, 0.70f, 0.90f, 1.00f);
        colours[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colours[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colours[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
        
        style.WindowPadding                     = ImVec2(10.0f, 10.0f);
        style.FramePadding                      = ImVec2(8.0f, 4.0f);
        style.ItemSpacing                       = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing                  = ImVec2(6.0f, 4.0f);
        style.IndentSpacing                     = 22.0f;
        style.ScrollbarSize                     = 14.0f;
        style.GrabMinSize                       = 10.0f;
        
        style.WindowRounding                    = 6.0f;
        style.ChildRounding                     = 4.0f;
        style.FrameRounding                     = 4.0f;
        style.PopupRounding                     = 4.0f;
        style.ScrollbarRounding                 = 6.0f;
        style.GrabRounding                      = 3.0f;
        style.TabRounding                       = 4.0f;
        
        style.WindowBorderSize                  = 1.0f;
        style.ChildBorderSize                   = 1.0f;
        style.PopupBorderSize                   = 1.0f;
        style.FrameBorderSize                   = 0.0f;
        style.TabBorderSize                     = 0.0f;
    }
}