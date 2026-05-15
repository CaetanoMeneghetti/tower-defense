#include "engine/hud.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

Hud::Hud() {}

Hud::~Hud() {}

void Hud::Init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr; 

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    SetupStyle();
}

void Hud::SetupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // CORES
    style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.12f, 0.08f, 0.05f, 0.95f);
    style.Colors[ImGuiCol_Border]        = ImVec4(0.40f, 0.25f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_Text]          = ImVec4(0.90f, 0.85f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.15f, 0.08f, 0.04f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.25f, 0.12f, 0.05f, 1.00f);
    
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 2.0f;
}

void Hud::Render(const AppState& state, float fps) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | 
                             ImGuiWindowFlags_NoSavedSettings | 
                             ImGuiWindowFlags_NoCollapse | 
                             ImGuiWindowFlags_NoMove;

    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);

    if (ImGui::Begin("1346AD: Iron & Blood", nullptr, flags)) {
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Separator();
        
        if (state.useFreeCamera) {
            ImGui::Text("Camera: Livre [C]");
            ImGui::Text("Yaw: %.2f | Pitch: %.2f", state.yaw, state.pitch);
        } else {
            ImGui::Text("Camera: Orbital [C]");
            ImGui::Text("Raio: %.2f", state.orbitRadius);
        }
        
        ImGui::Spacing();
        ImGui::Text("Curva Guia [T]: %s", state.showCurve ? "ON" : "OFF");
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Hud::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}