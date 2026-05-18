#include "engine/hud.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "game/game_constants.h"

Hud::Hud() {}
Hud::~Hud() {}

void Hud::init(GLFWwindow *window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  setupStyle();
}

void Hud::setTextures(const HudTextures &textures) {
  textures_ = textures;
}

void Hud::setupStyle() {
  ImGuiStyle &style = ImGui::GetStyle();
  style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.12f, 0.08f, 0.05f, 0.95f);
  style.Colors[ImGuiCol_Border]        = ImVec4(0.40f, 0.25f, 0.15f, 1.00f);
  style.Colors[ImGuiCol_Text]          = ImVec4(0.90f, 0.85f, 0.75f, 1.00f);
  style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.15f, 0.08f, 0.04f, 1.00f);
  style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.25f, 0.12f, 0.05f, 1.00f);

  style.WindowRounding = 0.0f;
  style.WindowBorderSize = 2.0f;
}

void Hud::render(AppState &state, float fps) {
  using game_constants::kArcherCost;
  using game_constants::kArquebusCost;

  // Nota: ImGui::NewFrame() e ImGui::Render() agora são chamados pelo main,
  // para permitir empilhar a HUD de upgrade entre eles
  // (ver game/upgrade_system.h).

  ImGuiWindowFlags topFlags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;

  float topBarHeight = 90.0f;  // Espaço para caber as molduras
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2((float)state.fbWidth, topBarHeight), ImGuiCond_Always);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

  if (ImGui::Begin("TopBar", nullptr, topFlags)) {
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetWindowPos();
    ImVec2 pMax = ImVec2(p0.x + state.fbWidth, p0.y + topBarHeight);

    // Mapa de fundo
    drawList->AddImage((void *)(intptr_t)textures_.topBackground, p0, pMax);

    // Overlay de borda
    drawList->AddRect(p0, pMax, IM_COL32(60, 40, 20, 200), 0.0f, 0, 4.0f);

    // -----------------------------------------------------------------------
    // Área de status (vida + ouro) com fundo de contraste
    // -----------------------------------------------------------------------
    ImVec2 statusAreaPos = ImVec2(p0.x + 10, p0.y + 10);
    ImVec2 statusAreaSize = ImVec2(220, topBarHeight - 20);

    drawList->AddRectFilled(
        statusAreaPos,
        ImVec2(statusAreaPos.x + statusAreaSize.x, statusAreaPos.y + statusAreaSize.y),
        IM_COL32(0, 0, 0, 150),
        10.0f);  // 10.0f = arredondamento

    ImGui::SetCursorPos(ImVec2(25.0f, 20.0f));
    ImGui::Image((void *)(intptr_t)textures_.healthIcon, ImVec2(24, 24));
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%d", state.health);

    ImGui::SetCursorPos(ImVec2(25.0f, 50.0f));
    ImGui::Image((void *)(intptr_t)textures_.goldIcon, ImVec2(24, 24));
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.1f, 1.0f), "%d GP", state.gold);

    // -----------------------------------------------------------------------
    // Seletor de torres (3 slots)
    // -----------------------------------------------------------------------
    const float itemWidth = 80.0f;
    const float totalWidth = itemWidth * 3;
    const float startX = (state.fbWidth / 2.0f) - (totalWidth / 2.0f);

    for (int i = 0; i < 3; i++) {
      float slotX = startX + (i * itemWidth);

      // Moldura do slot
      drawList->AddRectFilled(ImVec2(slotX + 5, p0.y + 10),
                              ImVec2(slotX + itemWidth - 5, p0.y + topBarHeight - 10),
                              IM_COL32(40, 30, 20, 180), 5.0f);

      // Separador vertical entre slots
      if (i < 2) {
        drawList->AddLine(ImVec2(slotX + itemWidth, p0.y + 15),
                          ImVec2(slotX + itemWidth, p0.y + topBarHeight - 15),
                          IM_COL32(100, 80, 50, 255), 2.0f);
      }

      // Arqueiro (slot 0)
      if (i == 0) {
        ImGui::SetCursorPos(
            ImVec2(slotX + (itemWidth - 60.0f) / 2.0f, (topBarHeight - 60.0f) / 2.0f));
        if (ImGui::ImageButton(
                "btn_archer", (void *)(intptr_t)textures_.archerIcon, ImVec2(50, 50))) {
          if (state.gold >= kArcherCost) {
            state.isPlacingTroop = true;
            state.selectedTroopType = 1;  // 1 = Arqueiro
          }
        }
      }

      // Arcabuz (slot 1)
      if (i == 1) {
        ImGui::SetCursorPos(
            ImVec2(slotX + (itemWidth - 60.0f) / 2.0f, (topBarHeight - 60.0f) / 2.0f));
        if (ImGui::ImageButton(
                "btn_arquebus", (void *)(intptr_t)textures_.arquebusIcon, ImVec2(50, 50))) {
          if (state.gold >= kArquebusCost) {
            state.isPlacingTroop = true;
            state.selectedTroopType = 2;
          }
        }
      }
    }

    // -----------------------------------------------------------------------
    // Info da wave (placeholder)
    // -----------------------------------------------------------------------
    ImGui::SetCursorPos(ImVec2(state.fbWidth - 150.0f, 35.0f));
    ImGui::Text("ONDADA: 1 / 10");
  }
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();

  // =========================================================================
  // HUD DE DEBUG
  // =========================================================================
  ImGuiWindowFlags debugFlags = ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_NoCollapse |
                                ImGuiWindowFlags_NoMove;

  ImGui::SetNextWindowPos(ImVec2(10.0f, topBarHeight + 10.0f), ImGuiCond_Always);

  if (ImGui::Begin("Debug", nullptr, debugFlags)) {
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
}

void Hud::shutdown() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
