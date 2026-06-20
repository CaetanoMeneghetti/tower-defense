#include "game/upgrade_system.h"

#include <imgui.h>

#include <cstdio>

namespace {
struct UpgradeEntry { int cost, dmg; float range, fireRate; const char *desc; };

// Tabela de upgrades: [nível atual 1..4] → stats do próximo nível (2..5).
const UpgradeEntry kArcher[4] = {
    {50,  12, 18.0f, 1.8f, "Nivel 2: Flechas de Ponta Larga\nAumenta levemente o dano base e o alcance."},
    {100, 14, 18.0f, 1.35f, "Nivel 3: Treinamento de Cadencia\nOs arqueiros disparam consideravelmente mais rapido. Excelente contra hordas."},
    {150, 22, 24.0f, 1.35f, "Nivel 4: Arcos de Madeira Composta\nEquipamento superior que garante projeteis mais rapidos e dano letal."},
    {200, 28, 28.0f, 0.85f, "Nivel 5: Mestres Arqueiros\nTreinamento de elite diminui o tempo entre disparos e"},
};
const UpgradeEntry kArquebus[4] = {
    {100,  25, 16.0f, 1.8f, "Nivel 2: Polvora Refinada\nReduz os detritos no cano, aumentando ligeiramente a cadencia de disparo."},
    {150, 55, 16.0f, 1.8f, "Nivel 3: Balas de Chumbo Pesadas\nBalas de chumbo causam dano massivo."},
    {250, 110, 22.0f, 1.8f, "Nivel 4: Cano Longo Estriado\nMelhora a precisao a longas distancias, permitindo atirar um pouco mais longe com forca total."},
    {400, 125, 22.0f, 1.0f, "Nivel 5: Mecanismo Flintlock\nMecanismo mais sofisticado de ignição aumenta a taxa de disparo significamente."},
};
const UpgradeEntry kCannon[4] = {
    {150, 55, 14.0f, 2.5f, "Nivel 2: Polvora Grossa.\nCarga mais potente aumenta o dano da bala de canhao e o alcance ligeiramente."},
    {250, 100, 14.0f, 2.5f, "Nivel 3: Bala de Ferro Macico.\nProjetil mais denso rasga qualquer armadura. Dano expressivo."},
    {450, 140, 18.0f, 2.0f, "Nivel 4: Treinamento de artilharia\nDiminui o tempo de recarregar."},
    {600, 220, 30.0f, 2.0f, "Nivel 5: Artilharia de cerco.\nCanhão avançado que causa dano massivo e possui alcance superior"},
};
const UpgradeEntry kCommander[4] = {
    {50,  1, 0.0f, 8.0f, "Nivel 2: Grito de guerra\nDiminui o intervalo de tempo entre chamados."},
    {250, 2, 0.0f, 7.8f, "Nivel 3: Treinamento em massa\n Aumenta a qunatidade de cavaleiros chamados"},
    {300, 2, 0.0f, 7.0f, "Nivel 4: Comandante\nDiminui o intervalo de tempo entre chamados."},
    {500, 3, 0.0f, 7.0f, "Nivel 5: General\n Chama um trio de cavaleiros a cada chamado."},
};
}  // namespace

UpgradeData calculateUpgrade(int troopType, int currentLevel) {
  const UpgradeEntry *tbl = (troopType == 1) ? kArcher
                          : (troopType == 2) ? kArquebus
                          : (troopType == 3) ? kCommander
                          : (troopType == 4) ? kCannon
                                             : nullptr;
  if (!tbl || currentLevel < 1 || currentLevel > 4)
    return {9999, 0, 0.0f, 0.0f, "Upgrade nao encontrado."};  // fallback seguro

  const UpgradeEntry &e = tbl[currentLevel - 1];
  return {e.cost, e.dmg, e.range, e.fireRate, e.desc};
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
  const char *troopName = (troop.type == 1) ? "Arqueiro"
                        : (troop.type == 4) ? "Canhoneiro"
                        : (troop.type == 3) ? "Comandante"
                                            : "Arcabuzeiro";
  ImGui::SetWindowFontScale(1.2f);
  ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", troopName);
  ImGui::Text("Nivel: %d %s", troop.level, troop.level >= kMaxTroopLevel ? "(MAXIMO)" : "");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::EndGroup();

  ImGui::Separator();

  const bool isMaxLevel = (troop.level >= kMaxTroopLevel);

  if (isMaxLevel) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "UNIDADE NO NIVEL MAXIMO");
    ImGui::Spacing();

    if (troop.type == 3) {
      ImGui::Text("Cavaleiros por invocacao: %d", troop.damage);
    } else {
      ImGui::Text("Dano Final: %d", troop.damage);
      ImGui::Text("Alcance Final: %.1f", troop.range);
      ImGui::Text("Recarga Final: %.1f seg", troop.fireRate);
    }

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

    if (troop.type == 3) {
      drawStatRow("Cavaleiros", troop.damage, up.nextDamage, false);
    } else {
      drawStatRow("Dano", troop.damage, up.nextDamage, false);
      drawStatRow("Alcance", troop.range, up.nextRange, true);
      drawStatRow("Recarga", troop.fireRate, up.nextFireRate, true);
    }

    ImGui::Separator();
    ImGui::SetCursorPosY(windowSize.y - 65);

    const bool canUpgrade = (state.gold >= up.cost);
    if (!canUpgrade) ImGui::BeginDisabled();

    ImGui::Image((void *)(intptr_t)placeholderIcon, ImVec2(50, 50));
    ImGui::SameLine();

    char btnText[64];
    std::snprintf(btnText, sizeof(btnText), "MELHORAR  -  %dg", up.cost);

    // Botão verde (mesma identidade do COMPRAR no menu de loja).
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.45f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.27f, 0.58f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.33f, 0.66f, 0.27f, 1.0f));
    if (ImGui::Button(btnText, ImVec2(-1, 50))) {
      state.gold -= up.cost;
      troop.damage = up.nextDamage;
      troop.range = up.nextRange;
      troop.fireRate = up.nextFireRate;
      didUpgrade = true;
    }
    ImGui::PopStyleColor(3);

    if (!canUpgrade) ImGui::EndDisabled();
  }

  ImGui::End();

  return didUpgrade;
}
