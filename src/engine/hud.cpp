#include "engine/hud.h"

#include <cstdio>
#include <filesystem>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "engine/audio.h"
#include "game/game_constants.h"
#include "game/wave_system.h"
#include "spell/spell_mode.h"  // spell::kSpellColors (cores dos feitiços)

// ---------------------------------------------------------------------------
// Helpers de cor
// ---------------------------------------------------------------------------
static const ImVec4 kColorGold  = ImVec4(1.00f, 0.85f, 0.20f, 1.0f);
static const ImVec4 kColorRed   = ImVec4(1.00f, 0.28f, 0.28f, 1.0f);
static const ImVec4 kColorCream = ImVec4(0.92f, 0.88f, 0.78f, 1.0f);
static const ImVec4 kColorMuted = ImVec4(0.65f, 0.60f, 0.50f, 1.0f);
static const ImVec4 kColorGreen = ImVec4(0.40f, 0.90f, 0.40f, 1.0f);

// ---------------------------------------------------------------------------
Hud::Hud() {}
Hud::~Hud() {}

void Hud::init(GLFWwindow *window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;

  // Fonte Cinzel (serifada medieval) como padrão de TODOS os textos do jogo.
  // A primeira fonte adicionada vira a default do ImGui. As faixas de glifos
  // padrão (0x20-0xFF) já cobrem os acentos do português. Se o arquivo ainda
  // não estiver presente, o ImGui cai na fonte embutida automaticamente.
  {
    namespace fs = std::filesystem;
    const char *fontCandidates[] = {
        // "data/fonts/Cinzel-Regular.ttf",
        "data/fonts/Cinzel-VariableFont_wght.ttf",
        // "data/fonts/Cinzel.ttf",
    };
    const float kFontSizePx = 18.0f;
    bool loaded = false;
    for (const char *path : fontCandidates) {
      if (fs::exists(path)) {
        if (io.Fonts->AddFontFromFileTTF(path, kFontSizePx) != nullptr) {
          loaded = true;
          break;
        }
      }
    }
    if (!loaded) {
      std::printf("[hud] Cinzel nao encontrada em data/fonts/; usando fonte padrao.\n");
    }
  }

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  setupStyle();
}

void Hud::setTextures(const HudTextures &textures) {
  textures_ = textures;
}

void Hud::setupStyle() {
  ImGuiStyle &s = ImGui::GetStyle();

  // Tema escuro neutro com acentos dourados (alinhado ao painel da célula 7).
  s.Colors[ImGuiCol_WindowBg]        = ImVec4(0.09f, 0.09f, 0.11f, 0.96f);
  s.Colors[ImGuiCol_ChildBg]         = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
  s.Colors[ImGuiCol_Border]          = ImVec4(0.55f, 0.40f, 0.14f, 0.90f);
  s.Colors[ImGuiCol_Text]            = kColorCream;
  s.Colors[ImGuiCol_TextDisabled]    = kColorMuted;
  s.Colors[ImGuiCol_TitleBg]         = ImVec4(0.12f, 0.09f, 0.04f, 1.00f);
  s.Colors[ImGuiCol_TitleBgActive]   = ImVec4(0.20f, 0.14f, 0.05f, 1.00f);
  s.Colors[ImGuiCol_Button]          = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
  s.Colors[ImGuiCol_ButtonHovered]   = ImVec4(0.45f, 0.32f, 0.10f, 1.00f);
  s.Colors[ImGuiCol_ButtonActive]    = ImVec4(0.58f, 0.42f, 0.13f, 1.00f);
  s.Colors[ImGuiCol_Separator]       = ImVec4(0.45f, 0.33f, 0.12f, 0.70f);
  s.Colors[ImGuiCol_FrameBg]         = ImVec4(0.16f, 0.16f, 0.19f, 1.00f);
  s.Colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
  s.Colors[ImGuiCol_PopupBg]         = ImVec4(0.10f, 0.10f, 0.12f, 0.98f);

  s.WindowRounding   = 8.0f;
  s.ChildRounding    = 6.0f;
  s.FrameRounding    = 5.0f;
  s.PopupRounding    = 6.0f;
  s.WindowBorderSize = 1.5f;
  s.FrameBorderSize  = 0.0f;
  s.ItemSpacing      = ImVec2(8.0f, 6.0f);
  s.WindowPadding    = ImVec2(12.0f, 10.0f);
}

// ---------------------------------------------------------------------------
// Barra superior
// ---------------------------------------------------------------------------
void Hud::renderTopBar(AppState &state, const WaveState &ws) {
  const float barH = 80.0f;
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse;

  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2((float)state.fbWidth, barH), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

  if (ImGui::Begin("TopBar", nullptr, flags)) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 p0(0, 0), p1((float)state.fbWidth, barH);

    dl->AddImage((void *)(intptr_t)textures_.topBackground, p0, p1);
    dl->AddLine(ImVec2(0, barH - 1), ImVec2((float)state.fbWidth, barH - 1),
                IM_COL32(160, 110, 30, 200), 2.0f);

    // ---- Status: vida + ouro ----
    const float sx = 12.0f, sy = 8.0f;
    dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + 170, barH - sy),
                      IM_COL32(0, 0, 0, 155), 6.0f);

    ImGui::SetCursorPos(ImVec2(sx + 8, 14.0f));
    ImGui::Image((void *)(intptr_t)textures_.healthIcon, ImVec2(20, 20));
    ImGui::SameLine();
    ImGui::SetCursorPosY(16.0f);
    ImGui::TextColored(kColorRed, "%d HP", state.health);

    ImGui::SetCursorPos(ImVec2(sx + 8, 44.0f));
    ImGui::Image((void *)(intptr_t)textures_.goldIcon, ImVec2(20, 20));
    ImGui::SameLine();
    ImGui::SetCursorPosY(46.0f);
    ImGui::TextColored(kColorGold, "%d GP", state.gold);

    // ---- Onda + inimigos (lado direito, estilo cabeçalho) ----
    const float rPanelW = 210.0f;
    const float rx = state.fbWidth - rPanelW - 12.0f;
    dl->AddRectFilled(ImVec2(rx, sy), ImVec2(rx + rPanelW, barH - sy),
                      IM_COL32(0, 0, 0, 155), 6.0f);

    if (ws.phase == WavePhase::Victory) {
      ImGui::SetCursorPos(ImVec2(rx + 14, 30.0f));
      ImGui::TextColored(kColorGold, "VIT\xc3\x93RIA!");
    } else {
      const WaveDef &wd = kWaves[ws.currentWave];
      char roundStr[24], enemyStr[28];
      std::snprintf(roundStr, sizeof(roundStr), "ONDA %d / 10", ws.currentWave + 1);
      std::snprintf(enemyStr, sizeof(enemyStr), "Mortos %d / %d", ws.killedCount, wd.enemyCount);
      ImGui::SetCursorPos(ImVec2(rx + 14, 16.0f));
      ImGui::TextColored(kColorGold, "%s", roundStr);
      ImGui::SetCursorPos(ImVec2(rx + 14, 46.0f));
      ImGui::TextColored(kColorCream, "%s", enemyStr);
    }

    // ---- Dica central: abrir o menu de compra ----
    const char *hint = "[M] Menu de Compra";
    float hintW = ImGui::CalcTextSize(hint).x;
    dl->AddRectFilled(ImVec2((state.fbWidth - hintW) * 0.5f - 12.0f, sy),
                      ImVec2((state.fbWidth + hintW) * 0.5f + 12.0f, barH - sy),
                      IM_COL32(0, 0, 0, 130), 6.0f);
    ImGui::SetCursorPos(ImVec2((state.fbWidth - hintW) * 0.5f, 30.0f));
    ImGui::TextColored(kColorGold, "%s", hint);
  }
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
}

// ---------------------------------------------------------------------------
// Desenha o ícone perfeito de uma forma de feitiço.
// shape: 0=círculo, 1=quadrado, 2=triângulo.
// ---------------------------------------------------------------------------
static void drawSpellIcon(ImDrawList *dl, int shape, ImVec2 c, float r, ImU32 col) {
  if (shape == 1) {
    dl->AddRectFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 3.0f);
  } else if (shape == 2) {
    ImVec2 v0(c.x, c.y - r);
    ImVec2 v1(c.x + 0.8660254f * r, c.y + 0.5f * r);
    ImVec2 v2(c.x - 0.8660254f * r, c.y + 0.5f * r);
    dl->AddTriangleFilled(v0, v1, v2, col);
  } else {
    dl->AddCircleFilled(c, r, col, 40);
  }
}

// ---------------------------------------------------------------------------
// Menu de compra (tecla M) — estilo "loja" (ver Spec/imagem1, célula 7).
// Abas Aliados/Feitiços à esquerda, cards das opções, painel de detalhe com
// botão COMPRAR. Comprar aliado entra no modo de posicionamento e fecha o menu;
// comprar feitiço adiciona 1 carga (consumível) e mantém o menu aberto.
// ---------------------------------------------------------------------------
void Hud::renderBuyMenu(AppState &state) {
  if (!state.showBuyMenu) return;
  using namespace game_constants;

  struct AllyOpt  { const char *name; unsigned int icon; int cost; int type; const char *desc; };
  struct SpellOpt { const char *name; int cls; int cost; const char *desc; };
  const AllyOpt allies[4] = {
      {"Arqueiro",    textures_.archerIcon,   kArcherCost,   1, "Unidade b\xc3\xa1sica. Dano \xc3\xbanico \xc3\xa0 dist\xc3\xa2ncia."},
      {"Arcabuzeiro", textures_.arquebusIcon, kArquebusCost, 2, "Tiro pesado. Dano alto, recarga lenta."},
      {"Canhoneiro",  textures_.cannonIcon,   kCannonCost,   4, "Dano em \xc3\xa1rea. Devastador contra grupos."},
      {"Comandante",  textures_.knightIcon,   kKnightCost,   3, "Invoca cavaleiros que avan\xc3\xa7""am pelo caminho."},
  };
  const SpellOpt spells[3] = {
      {"Lentid\xc3\xa3o", 1, kSpellSquareCost,   "Inimigos na \xc3\xa1rea andam a 75% da velocidade."},
      {"Dano",           2, kSpellTriangleCost, "Inimigos na \xc3\xa1rea perdem metade da vida."},
      {"Veneno",         0, kSpellCircleCost,   "Inimigos na \xc3\xa1rea: veneno 10%/s por 8s."},
  };

  const bool isAlly = (state.buyMenuTab == 0);
  const int  nCards = isAlly ? 4 : 3;
  if (state.buyMenuSelection >= nCards) state.buyMenuSelection = 0;

  ImGuiIO &io = ImGui::GetIO();
  const float W = io.DisplaySize.x, Hh = io.DisplaySize.y;

  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(W, Hh), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoMove;

  if (ImGui::Begin("##BuyMenu", nullptr, flags)) {
    ImDrawList *dl = ImGui::GetWindowDrawList();

    // Fundo escurecido (foca o painel e bloqueia cliques no mundo)
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, Hh), IM_COL32(0, 0, 0, 150));

    // Painel central
    const float panelW = 760.0f, panelH = 430.0f;
    const float px = (W - panelW) * 0.5f, py = (Hh - panelH) * 0.5f;
    dl->AddRectFilled(ImVec2(px, py), ImVec2(px + panelW, py + panelH), IM_COL32(22, 22, 27, 252), 10.0f);
    dl->AddRect(ImVec2(px, py), ImVec2(px + panelW, py + panelH), IM_COL32(150, 110, 35, 255), 10.0f, 0, 2.0f);

    // ---- Header ----
    const float headH = 50.0f;
    ImGui::SetCursorPos(ImVec2(px + 20, py + 14));
    ImGui::SetWindowFontScale(1.15f);
    ImGui::TextColored(kColorGold, "MENU DE COMPRA");
    ImGui::SetWindowFontScale(1.0f);

    char goldStr[16];
    std::snprintf(goldStr, sizeof(goldStr), "%d", state.gold);
    float gW = ImGui::CalcTextSize(goldStr).x;
    ImGui::SetCursorPos(ImVec2(px + panelW - 42.0f - gW, py + 16));
    ImGui::Image((void *)(intptr_t)textures_.goldIcon, ImVec2(20, 20));
    ImGui::SameLine(0, 6);
    ImGui::SetCursorPosY(py + 16);
    ImGui::TextColored(kColorGold, "%s", goldStr);

    dl->AddLine(ImVec2(px + 16, py + headH), ImVec2(px + panelW - 16, py + headH),
                IM_COL32(120, 90, 30, 160), 1.5f);

    // ---- Abas (coluna esquerda) ----
    const float tabX = px + 16, tabY = py + headH + 16, tabW = 130, tabH = 44, tabGap = 8;
    const char *tabNames[2] = {"ALIADOS", "FEITI\xc3\x87OS"};
    for (int t = 0; t < 2; ++t) {
      float ty = tabY + t * (tabH + tabGap);
      bool active = (state.buyMenuTab == t);
      dl->AddRectFilled(ImVec2(tabX, ty), ImVec2(tabX + tabW, ty + tabH),
                        active ? IM_COL32(72, 54, 16, 255) : IM_COL32(34, 34, 40, 255), 6.0f);
      dl->AddRect(ImVec2(tabX, ty), ImVec2(tabX + tabW, ty + tabH),
                  active ? IM_COL32(212, 162, 42, 255) : IM_COL32(70, 70, 80, 200),
                  6.0f, 0, active ? 2.0f : 1.0f);
      ImGui::SetCursorPos(ImVec2(tabX + 16, ty + (tabH - ImGui::GetTextLineHeight()) * 0.5f));
      ImGui::TextColored(active ? kColorGold : kColorCream, "%s", tabNames[t]);
      ImGui::SetCursorPos(ImVec2(tabX, ty));
      char tid[16];
      std::snprintf(tid, sizeof(tid), "##tab%d", t);
      if (ImGui::InvisibleButton(tid, ImVec2(tabW, tabH)) && state.buyMenuTab != t) {
        state.buyMenuTab = t;
        state.buyMenuSelection = 0;
      }
    }

    // ---- Cards das opções ----
    const float cardsX = tabX + tabW + 22;
    const float cardsY = py + headH + 16;
    const float cardW = 112, cardH = 175, cardGap = 14;

    for (int i = 0; i < nCards; ++i) {
      float cx = cardsX + i * (cardW + cardGap);
      float cy = cardsY;
      bool selected = (state.buyMenuSelection == i);
      int cost = isAlly ? allies[i].cost : spells[i].cost;
      bool canAfford = state.gold >= cost;

      dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                        IM_COL32(30, 30, 36, 255), 7.0f);
      dl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                  selected ? IM_COL32(212, 162, 42, 255) : IM_COL32(70, 70, 80, 220),
                  7.0f, 0, selected ? 2.5f : 1.0f);

      // Nome (topo, centrado)
      const char *cname = isAlly ? allies[i].name : spells[i].name;
      float nW = ImGui::CalcTextSize(cname).x;
      ImGui::SetCursorPos(ImVec2(cx + (cardW - nW) * 0.5f, cy + 8));
      ImGui::TextColored(kColorCream, "%s", cname);

      // Moldura do retrato
      ImVec2 pBox0(cx + 10, cy + 32), pBox1(cx + cardW - 10, cy + cardH - 34);
      dl->AddRectFilled(pBox0, pBox1, IM_COL32(15, 15, 18, 255), 4.0f);
      dl->AddRect(pBox0, pBox1, IM_COL32(90, 70, 25, 200), 4.0f, 0, 1.0f);

      if (isAlly) {
        const float imgSz = 84.0f;
        ImGui::SetCursorPos(ImVec2((pBox0.x + pBox1.x) * 0.5f - imgSz * 0.5f,
                                   (pBox0.y + pBox1.y) * 0.5f - imgSz * 0.5f));
        ImGui::Image((void *)(intptr_t)allies[i].icon, ImVec2(imgSz, imgSz));
      } else {
        int cls = spells[i].cls;
        const float *rgb = spell::kSpellColors[cls];
        ImU32 col = IM_COL32(int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255),
                             canAfford ? 255 : 120);
        drawSpellIcon(dl, cls, ImVec2((pBox0.x + pBox1.x) * 0.5f, (pBox0.y + pBox1.y) * 0.5f),
                      32.0f, col);
        // Badge de cargas
        int charges = state.spellCharges[cls];
        if (charges > 0) {
          char cnt[8];
          std::snprintf(cnt, sizeof(cnt), "x%d", charges);
          ImVec2 ts = ImGui::CalcTextSize(cnt);
          ImVec2 tp(pBox1.x - ts.x - 6.0f, pBox0.y + 5.0f);
          dl->AddRectFilled(ImVec2(tp.x - 3, tp.y - 1), ImVec2(tp.x + ts.x + 3, tp.y + ts.y + 1),
                            IM_COL32(0, 0, 0, 190), 3.0f);
          dl->AddText(tp, IM_COL32(255, 230, 140, 255), cnt);
        }
      }

      // Custo (rodapé do card, com moeda)
      char costStr[12];
      std::snprintf(costStr, sizeof(costStr), "%d", cost);
      float coinW = 16.0f, cW = ImGui::CalcTextSize(costStr).x;
      float totalW = coinW + 4.0f + cW;
      float startX = cx + (cardW - totalW) * 0.5f;
      ImGui::SetCursorPos(ImVec2(startX, cy + cardH - 26));
      ImGui::Image((void *)(intptr_t)textures_.goldIcon, ImVec2(coinW, coinW));
      ImGui::SameLine(0, 4);
      ImGui::SetCursorPosY(cy + cardH - 25);
      ImGui::TextColored(canAfford ? kColorGold : kColorRed, "%s", costStr);

      // Clique seleciona o card
      ImGui::SetCursorPos(ImVec2(cx, cy));
      char cid[16];
      std::snprintf(cid, sizeof(cid), "##card%d", i);
      if (ImGui::InvisibleButton(cid, ImVec2(cardW, cardH))) {
        state.buyMenuSelection = i;
      }
    }

    // ---- Painel de detalhe (rodapé) ----
    const float detY = cardsY + cardH + 16;
    const float detX = cardsX;
    const float detW = px + panelW - 16 - detX;
    dl->AddLine(ImVec2(detX, detY - 6), ImVec2(detX + detW, detY - 6),
                IM_COL32(120, 90, 30, 120), 1.0f);

    int sel = state.buyMenuSelection;
    const char *selName = isAlly ? allies[sel].name : spells[sel].name;
    const char *selDesc = isAlly ? allies[sel].desc : spells[sel].desc;
    int selCost = isAlly ? allies[sel].cost : spells[sel].cost;
    bool canBuy = state.gold >= selCost;

    ImGui::SetCursorPos(ImVec2(detX, detY));
    ImGui::SetWindowFontScale(1.1f);
    ImGui::TextColored(kColorGold, "%s", selName);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SetCursorPos(ImVec2(detX, detY + 26));
    ImGui::PushTextWrapPos(detX + detW - 170.0f);
    ImGui::TextColored(kColorMuted, "%s", selDesc);
    ImGui::PopTextWrapPos();

    // Botão COMPRAR (verde) à direita
    const float btnW = 150.0f, btnH = 46.0f;
    ImGui::SetCursorPos(ImVec2(detX + detW - btnW, detY + 8));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.45f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.27f, 0.58f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.33f, 0.66f, 0.27f, 1.0f));
    if (!canBuy) ImGui::BeginDisabled();
    if (ImGui::Button("COMPRAR", ImVec2(btnW, btnH))) {
      if (isAlly) {
        state.isPlacingTroop    = true;
        state.selectedTroopType = allies[sel].type;
        state.showBuyMenu       = false;  // fecha pra liberar o campo de batalha
        audio::playOneShot("data/audio/selection_sound.mp3");
      } else {
        state.gold -= selCost;
        state.spellCharges[spells[sel].cls]++;
        audio::playOneShot("data/audio/selection_sound.mp3");
      }
    }
    if (!canBuy) ImGui::EndDisabled();
    ImGui::PopStyleColor(3);

    // Dica de fechar
    ImGui::SetCursorPos(ImVec2(px + 20, py + panelH - 24));
    ImGui::TextColored(kColorMuted, "[M] fecha o menu");
  }
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
}

// ---------------------------------------------------------------------------
// Legenda de atalhos — sempre visível (canto inferior esquerdo).
// ---------------------------------------------------------------------------
void Hud::renderControlsLegend(const AppState &state) {
  struct KeyHint { const char *key; const char *desc; };
  const KeyHint hints[] = {
      {"M",    "  Menu de compra"},
      {"C",    "  Trocar c\xc3\xa2mera"},
      {"F",    "  Desenhar feiti\xc3\xa7o"},
      {"T",    "  Curva (debug)"},
      {"Y",    "  Pular intermiss\xc3\xa3o"},
      {"WASD", "  Mover c\xc3\xa2mera"},
  };
  const int n = (int)(sizeof(hints) / sizeof(hints[0]));

  const float lineH = ImGui::GetTextLineHeightWithSpacing();
  const float pad = 10.0f;
  const float winW = 220.0f;
  const float winH = pad * 2.0f + lineH * (n + 1);

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs |
      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::SetNextWindowPos(ImVec2(10.0f, state.fbHeight - winH - 10.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.06f, 0.82f));
  ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.45f, 0.34f, 0.12f, 0.9f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));

  if (ImGui::Begin("##Controls", nullptr, flags)) {
    ImGui::TextColored(kColorGold, "ATALHOS");
    for (int i = 0; i < n; ++i) {
      // F (desenho de feitiço) só funciona na câmera aérea: esmaecido fora dela.
      const bool inactive = (hints[i].key[0] == 'F' && state.cameraMode != CameraMode::Aerial);
      ImGui::TextColored(inactive ? kColorMuted : kColorGold, "[%s]", hints[i].key);
      ImGui::SameLine(58.0f);
      ImGui::TextColored(inactive ? kColorMuted : kColorCream, "%s", hints[i].desc);
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);
}

// ---------------------------------------------------------------------------
// Barra de wave — horizontal, abaixo da top bar
// ---------------------------------------------------------------------------
void Hud::renderWaveBar(const AppState &state, const WaveState &ws) {
  const float topBarH = 80.0f;
  const float barH    = 32.0f;

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoInputs;

  ImGui::SetNextWindowPos(ImVec2(0, topBarH), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2((float)state.fbWidth, barH), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));  // caixa desenhada à mão

  if (ImGui::Begin("WaveBar", nullptr, flags)) {
    ImDrawList *dl = ImGui::GetWindowDrawList();

    // Caixa arredondada com o lado esquerdo alinhado à caixa de Debug (x=10) e
    // margem simétrica à direita.
    const float margin = 10.0f;
    const float boxL = margin;
    const float boxR = (float)state.fbWidth - margin;
    const float boxW = boxR - boxL;
    const ImVec2 boxP0(boxL, topBarH + 2.0f), boxP1(boxR, topBarH + barH - 2.0f);
    dl->AddRectFilled(boxP0, boxP1, IM_COL32(14, 12, 8, 224), 5.0f);
    dl->AddRect(boxP0, boxP1, IM_COL32(90, 60, 15, 160), 5.0f, 0, 1.0f);

    if (ws.phase == WavePhase::Victory) {
      const char *v = "VICT\xc3\x93RIA!  Todas as ondas foram derrotadas.";
      ImGui::SetCursorPos(ImVec2(boxL + (boxW - ImGui::CalcTextSize(v).x) * 0.5f, 7.0f));
      ImGui::TextColored(kColorGold, "%s", v);
    } else {
      const WaveDef &def = kWaves[ws.currentWave];

      // Rótulo da onda (esquerda)
      char lbl[24];
      std::snprintf(lbl, sizeof(lbl), "ONDA %d / 10", ws.currentWave + 1);
      const float lblW = 96.0f;
      ImGui::SetCursorPos(ImVec2(boxL + 12.0f, 8.0f));
      ImGui::TextColored(kColorGold, "%s", lbl);

      // Texto de fase (direita)
      char rhs[40];
      if (ws.phase == WavePhase::Intermission) {
        int secs = static_cast<int>(ws.timer) + 1;
        std::snprintf(rhs, sizeof(rhs), "Em %ds  \xe2\x80\x94  [Y] Pular", secs);
      } else if (ws.phase == WavePhase::Starting) {
        std::snprintf(rhs, sizeof(rhs), "INICIANDO...");
      } else {
        std::snprintf(rhs, sizeof(rhs), "Mortos: %d / %d", ws.killedCount, def.enemyCount);
      }
      float rhsW = ImGui::CalcTextSize(rhs).x;
      ImGui::SetCursorPos(ImVec2(boxR - rhsW - 12.0f, 8.0f));
      ImVec4 rhsColor = (ws.phase == WavePhase::Intermission) ? kColorMuted
                      : (ws.phase == WavePhase::Starting)      ? kColorGold
                                                               : kColorGreen;
      ImGui::TextColored(rhsColor, "%s", rhs);

      // Barra de progresso (centro, toma o espaço restante dentro da caixa)
      const float barPad = 12.0f;
      const float bx     = boxL + 12.0f + lblW + barPad;
      const float bw     = (boxR - 12.0f - rhsW - barPad) - bx;
      const float by     = topBarH + 11.0f;
      const float bh     = 10.0f;

      float prog;
      if (ws.phase == WavePhase::Intermission) {
        float total = def.intermissionSecs;
        prog = (total > 0) ? (1.0f - ws.timer / total) : 1.0f;
      } else if (ws.phase == WavePhase::Starting) {
        prog = 1.0f - (ws.startingTimer / 5.0f);
      } else {
        int total = def.enemyCount;
        prog = (total > 0) ? static_cast<float>(ws.killedCount) / total : 1.0f;
      }
      if (prog < 0.0f) prog = 0.0f;
      if (prog > 1.0f) prog = 1.0f;

      if (bw > 1.0f) {
        // Trilho
        dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh),
                          IM_COL32(25, 16, 5, 220), 4.0f);
        // Preenchimento
        if (bw * prog > 1.0f) {
          ImU32 barColor = (ws.phase == WavePhase::Intermission) ? IM_COL32(60, 120, 200, 230)
                         : (ws.phase == WavePhase::Starting)      ? IM_COL32(200, 160, 30, 230)
                                                                  : IM_COL32(190, 65, 40, 230);
          dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw * prog, by + bh), barColor, 4.0f);
        }
        // Moldura
        dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh),
                    IM_COL32(90, 60, 15, 180), 4.0f);
      }
    }
  }
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
}

// ---------------------------------------------------------------------------
// Overlay de Intermission (centro da tela)
// ---------------------------------------------------------------------------
void Hud::renderIntermissionOverlay(const WaveState &ws) {
  if (ws.phase != WavePhase::Intermission) return;

  const WaveDef &def  = kWaves[ws.currentWave];
  const float topBarH = 80.0f;
  const float waveBarH = 32.0f;
  const float bannerY  = topBarH + waveBarH;   // logo abaixo das duas barras
  const float bannerH  = 54.0f;

  ImGuiIO &io = ImGui::GetIO();
  const float W = io.DisplaySize.x;

  ImGui::SetNextWindowPos(ImVec2(0.0f, bannerY), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(W, bannerH), ImGuiCond_Always);

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse;

  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.04f, 0.01f, 0.93f));
  ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.55f, 0.38f, 0.10f, 1.00f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));

  if (ImGui::Begin("##IntermissionBanner", nullptr, flags)) {
    ImDrawList *dl = ImGui::GetWindowDrawList();

    const float fontSize = ImGui::GetTextLineHeight();
    // Duas linhas de texto: centralizar o bloco verticalmente
    const float blockH  = fontSize * 2.0f + 4.0f;
    const float yTop    = (bannerH - blockH) * 0.5f - 1.0f;
    const float yBot    = yTop + fontSize + 4.0f;

    // ---- Seção esquerda: rótulo da onda ----
    const float leftW = 170.0f;
    ImGui::SetCursorPos(ImVec2(14.0f, yTop));
    ImGui::TextColored(kColorMuted, "PR\xc3\x93XIMA ONDA");
    ImGui::SetCursorPos(ImVec2(14.0f, yBot));
    ImGui::TextColored(kColorGold, "%s", def.label);

    // Separador esquerdo
    dl->AddLine(ImVec2(leftW, bannerY + 6.0f), ImVec2(leftW, bannerY + bannerH - 6.0f),
                IM_COL32(100, 70, 20, 140), 1.0f);

    // ---- Seção central: uma entrada por tipo de inimigo presente ----
    struct EnemyEntry {
      unsigned int tex;
      const char  *label;
      int          count;
      int          hp;
      float        speed;
      int          damage;
    };

    int normalCount = def.enemyCount - def.armoredCount - def.chargerCount - def.necroCount;

    EnemyEntry entries[4];
    int entryCount = 0;
    if (normalCount   > 0) entries[entryCount++] = { textures_.zombiePortrait,        "Zumbi",       normalCount,       def.enemyStats.maxHp,  def.enemyStats.speed,  def.enemyStats.damage };
    if (def.armoredCount > 0) entries[entryCount++] = { textures_.armoredZombiePortrait, "Blindado",    def.armoredCount,  def.armoredStats.maxHp, def.armoredStats.speed, def.armoredStats.damage };
    if (def.chargerCount > 0) entries[entryCount++] = { textures_.chargerPortrait,       "Corredor",    def.chargerCount,  def.chargerStats.maxHp, def.chargerStats.speed, def.chargerStats.damage };
    if (def.necroCount   > 0) entries[entryCount++] = { textures_.necromancerPortrait,   "Necromante",  def.necroCount,    def.necroStats.maxHp,   def.necroStats.speed,   def.necroStats.damage };

    const float iconSz   = 40.0f;
    const float entryW   = iconSz + 90.0f;   // ícone + texto ao lado
    const float iconY    = (bannerH - iconSz) * 0.5f;
    float curX           = leftW + 12.0f;

    for (int ei = 0; ei < entryCount; ++ei) {
      const EnemyEntry &en = entries[ei];

      // Separador entre entradas
      if (ei > 0) {
        dl->AddLine(ImVec2(curX - 6.0f, bannerY + 8.0f),
                    ImVec2(curX - 6.0f, bannerY + bannerH - 8.0f),
                    IM_COL32(80, 55, 15, 100), 1.0f);
      }

      // Fundo/borda do ícone
      ImVec2 ic0 = ImVec2(curX, bannerY + iconY);
      dl->AddRectFilled(ic0, ImVec2(ic0.x + iconSz, ic0.y + iconSz), IM_COL32(15, 10, 4, 220), 4.0f);
      dl->AddRect      (ic0, ImVec2(ic0.x + iconSz, ic0.y + iconSz), IM_COL32(130, 88, 18, 200), 4.0f, 0, 1.0f);

      ImGui::SetCursorPos(ImVec2(curX, iconY));
      ImGui::Image((void *)(intptr_t)en.tex, ImVec2(iconSz, iconSz));
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(kColorGold, "%s", en.label);
        ImGui::Separator();
        ImGui::Text("Quantidade : %d", en.count);
        ImGui::Text("Vida       : %d", en.hp);
        ImGui::Text("Velocidade : %.1f", en.speed);
        ImGui::Text("Dano       : %d", en.damage);
        ImGui::EndTooltip();
      }

      // Texto ao lado do ícone
      const float tx = curX + iconSz + 6.0f;
      ImGui::SetCursorPos(ImVec2(tx, yTop));
      ImGui::TextColored(kColorCream, "%s", en.label);
      ImGui::SetCursorPos(ImVec2(tx, yBot));
      ImGui::TextColored(kColorGold, "\xc3\x97 %d", en.count);

      curX += entryW;
    }

    // ---- Seção direita: countdown ----
    const float rightW = 160.0f;
    dl->AddLine(ImVec2(W - rightW, bannerY + 6.0f), ImVec2(W - rightW, bannerY + bannerH - 6.0f),
                IM_COL32(100, 70, 20, 140), 1.0f);

    int secs = static_cast<int>(ws.timer) + 1;
    char cd[16];
    std::snprintf(cd, sizeof(cd), "Em  %d s", secs);
    ImGui::SetCursorPos(ImVec2(W - rightW + 12.0f, yTop));
    ImGui::TextColored(kColorCream, "%s", cd);
    ImGui::SetCursorPos(ImVec2(W - rightW + 12.0f, yBot));
    ImGui::TextColored(kColorMuted, "[Y]  Pular");
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
}

// ---------------------------------------------------------------------------
// Overlay de Vitória
// ---------------------------------------------------------------------------
void Hud::renderVictoryOverlay() {
  // Mesma posição do banner de intermission — larga e baixa, acima do jogo
  const float bannerY = 80.0f + 32.0f;
  const float bannerH = 54.0f;

  ImGuiIO &io = ImGui::GetIO();
  const float W = io.DisplaySize.x;

  ImGui::SetNextWindowPos(ImVec2(0.0f, bannerY), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(W, bannerH), ImGuiCond_Always);

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;

  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.10f, 0.03f, 0.95f));
  ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.25f, 0.65f, 0.25f, 1.00f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));

  if (ImGui::Begin("##Victory", nullptr, flags)) {
    const float fontSize = ImGui::GetTextLineHeight();
    const float blockH   = fontSize * 2.0f + 4.0f;
    const float yTop     = (bannerH - blockH) * 0.5f - 1.0f;
    const float yBot     = yTop + fontSize + 4.0f;

    const char *v1 = "VICT\xc3\x93RIA!";
    ImGui::SetWindowFontScale(1.25f);
    float v1W = ImGui::CalcTextSize(v1).x;
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetCursorPos(ImVec2((W - v1W * 1.25f) * 0.5f, yTop));
    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextColored(kColorGold, "%s", v1);
    ImGui::SetWindowFontScale(1.0f);

    const char *v2 = "Todas as ondas foram derrotadas.";
    ImGui::SetCursorPos(ImVec2((W - ImGui::CalcTextSize(v2).x) * 0.5f, yBot));
    ImGui::TextColored(kColorGreen, "%s", v2);
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------
void Hud::renderDebugWindow(AppState &state, float fps) {
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;

  // Abaixo da top bar (80) + wave bar (32)
  ImGui::SetNextWindowPos(ImVec2(10.0f, 120.0f), ImGuiCond_Always);

  if (ImGui::Begin("Debug", nullptr, flags)) {
    ImGui::TextColored(kColorGold, "%.1f FPS", fps);
    ImGui::Separator();
    if (state.cameraMode == CameraMode::Free) {
      ImGui::TextColored(kColorMuted, "C\xc3\xa2mera Livre [C]");
      ImGui::Text("Yaw %.1f  Pitch %.1f", state.yaw, state.pitch);
    } else if (state.cameraMode == CameraMode::Aerial) {
      ImGui::TextColored(kColorMuted, "C\xc3\xa2mera A\xc3\xa9rea [C]");
    } else {
      ImGui::TextColored(kColorMuted, "C\xc3\xa2mera Orbital (clique em unidade)");
      ImGui::Text("Raio %.1f", state.orbitRadius);
    }
    ImGui::Spacing();
    ImGui::TextColored(kColorMuted, "Curva [T]: %s", state.showCurve ? "ON" : "OFF");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.80f, 0.30f, 0.30f, 1.0f));
    if (ImGui::Button("Matar zumbis  +999g")) state.debugSkipWave = true;
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.40f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.52f, 0.70f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.25f, 0.60f, 0.80f, 1.0f));
    if (ImGui::Button("Forcar Vitoria")) state.debugForceVictory = true;
    ImGui::PopStyleColor(3);
    ImGui::PopStyleColor(3);
  }
  ImGui::End();
}

// ---------------------------------------------------------------------------
void Hud::render(AppState &state, const WaveState &ws, float fps) {
  renderTopBar(state, ws);
  renderWaveBar(state, ws);
  renderIntermissionOverlay(ws);
  if (ws.phase == WavePhase::Victory) renderVictoryOverlay();
  renderControlsLegend(state);
  renderDebugWindow(state, fps);
  renderBuyMenu(state);  // overlay por cima de tudo
}

void Hud::shutdown() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
