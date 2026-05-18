#include "game/upgrade_system.h"

#include <imgui.h>

#include <cstdio>

UpgradeData calculateUpgrade(int troopType, int currentLevel) {
  UpgradeData data;

  // Fallback seguro
  data.cost = 9999;
  data.nextDamage = 0;
  data.nextRange = 0.0f;
  data.nextFireRate = 0.0f;
  data.description = "Upgrade nao encontrado.";

  if (troopType == 1) {
    // ---------------- ARQUEIRO ----------------
    switch (currentLevel) {
      case 1:  // 1 → 2
        data.cost = 50;
        data.nextDamage = 12;
        data.nextRange = 18.0f;
        data.nextFireRate = 1.8f;
        data.description =
            "Nivel 2: Flechas de Ponta Larga.\nAumenta levemente o dano base e o alcance para conter os primeiros inimigos.";
        break;
      case 2:  // 2 → 3
        data.cost = 120;
        data.nextDamage = 16;
        data.nextRange = 22.0f;
        data.nextFireRate = 1.5f;
        data.description =
            "Nivel 3: Treinamento de Cadencia.\nOs arqueiros disparam consideravelmente mais rapido. Excelente contra hordas.";
        break;
      case 3:  // 3 → 4
        data.cost = 250;
        data.nextDamage = 22;
        data.nextRange = 25.0f;
        data.nextFireRate = 1.2f;
        data.description =
            "Nivel 4: Arcos de Madeira Composta.\nEquipamento superior que garante projeteis mais rapidos e dano letal.";
        break;
      case 4:  // 4 → 5 (MÁXIMO)
        data.cost = 500;
        data.nextDamage = 35;
        data.nextRange = 30.0f;
        data.nextFireRate = 0.8f;
        data.description =
            "Nivel 5: Mestres Arqueiros.\nO auge da arquearia. Chuva de flechas implacavel com alcance cobrindo quase todo o campo.";
        break;
    }
  } else if (troopType == 2) {
    // ---------------- ARCABUZEIRO ----------------
    switch (currentLevel) {
      case 1:
        data.cost = 80;
        data.nextDamage = 25;
        data.nextRange = 16.0f;
        data.nextFireRate = 1.9f;
        data.description =
            "Nivel 2: Polvora Refinada.\nReduz os detritos no cano, aumentando ligeiramente a cadencia de disparo.";
        break;
      case 2:
        data.cost = 180;
        data.nextDamage = 45;
        data.nextRange = 17.0f;
        data.nextFireRate = 1.9f;
        data.description =
            "Nivel 3: Balas de Chumbo Pesadas.\nFoca inteiramente no impacto. Dano massivo capaz de rasgar armaduras medias.";
        break;
      case 3:
        data.cost = 350;
        data.nextDamage = 70;
        data.nextRange = 19.0f;
        data.nextFireRate = 1.8f;
        data.description =
            "Nivel 4: Cano Longo Estriado.\nMelhora a precisao a longas distancias, permitindo atirar um pouco mais longe com forca total.";
        break;
      case 4:
        data.cost = 700;
        data.nextDamage = 120;
        data.nextRange = 22.0f;
        data.nextFireRate = 1.5f;
        data.description =
            "Nivel 5: Mosqueteiros de Elite.\nArmamento de ponta. Cada tiro e um canhao portatil com tempo de recarga otimizado.";
        break;
    }
  }

  return data;
}

bool drawTroopDetailsHud(GameObject &troop, AppState &state, unsigned int placeholderIcon) {
  bool didUpgrade = false;

  const ImVec2 windowSize(450, 420);
  ImGui::SetNextWindowPos(
      ImVec2(state.fbWidth - windowSize.x - 20, state.fbHeight - windowSize.y - 20),
      ImGuiCond_Always);
  ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

  ImGui::Begin("TroopDetails", nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);

  ImGui::Image((void *)(intptr_t)placeholderIcon, ImVec2(70, 70));
  ImGui::SameLine();

  ImGui::BeginGroup();
  ImGui::SetWindowFontScale(1.2f);
  ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                     troop.type == 1 ? "Arqueiro" : "Arcabuzeiro");
  ImGui::Text("Nivel: %d %s", troop.level, troop.level >= kMaxTroopLevel ? "(MAXIMO)" : "");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::EndGroup();

  ImGui::Separator();

  const bool isMaxLevel = (troop.level >= kMaxTroopLevel);

  if (isMaxLevel) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "UNIDADE NO NIVEL MAXIMO");
    ImGui::Spacing();

    ImGui::Text("Dano Final: %d", troop.damage);
    ImGui::Text("Alcance Final: %.1f", troop.range);
    ImGui::Text("Recarga Final: %.1f seg", troop.fireRate);

    ImGui::Separator();

    ImGui::SetCursorPosY(windowSize.y - 65);
    ImGui::BeginDisabled();
    ImGui::Button("NIVEL MAXIMO ATINGIDO", ImVec2(-1, 50));
    ImGui::EndDisabled();
  } else {
    UpgradeData up = calculateUpgrade(troop.type, troop.level);

    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Proxima Melhoria:");
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8));
    ImGui::TextWrapped("%s", up.description.c_str());
    ImGui::PopStyleVar();
    ImGui::Spacing();

    auto drawStatRow = [](const char *label, float current, float next, bool isFloat) {
      ImGui::Text("%s", label);
      ImGui::SameLine(130);
      ImGui::Text(":");
      ImGui::SameLine(150);

      if (isFloat) {
        ImGui::Text("%.1f", current);
      } else {
        ImGui::Text("%d", (int)current);
      }

      ImGui::SameLine(210);
      ImGui::Text("->");
      ImGui::SameLine(250);

      if (isFloat) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%.1f", next);
      } else {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%d", (int)next);
      }
    };

    drawStatRow("Dano", troop.damage, up.nextDamage, false);
    drawStatRow("Alcance", troop.range, up.nextRange, true);
    drawStatRow("Recarga", troop.fireRate, up.nextFireRate, true);

    ImGui::Separator();
    ImGui::SetCursorPosY(windowSize.y - 65);

    const bool canUpgrade = (state.gold >= up.cost);
    if (!canUpgrade) ImGui::BeginDisabled();

    ImGui::Image((void *)(intptr_t)placeholderIcon, ImVec2(50, 50));
    ImGui::SameLine();

    char btnText[64];
    std::snprintf(btnText, sizeof(btnText), "UPGRADE - %dg", up.cost);

    if (ImGui::Button(btnText, ImVec2(-1, 50))) {
      state.gold -= up.cost;
      troop.damage = up.nextDamage;
      troop.range = up.nextRange;
      troop.fireRate = up.nextFireRate;
      didUpgrade = true;
    }

    if (!canUpgrade) ImGui::EndDisabled();
  }

  ImGui::End();

  return didUpgrade;
}
