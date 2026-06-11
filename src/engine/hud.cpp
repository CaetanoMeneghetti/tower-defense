#include "engine/hud.h"

#include <cstdio>

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

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  setupStyle();
}

void Hud::setTextures(const HudTextures &textures) {
  textures_ = textures;
}

void Hud::setupStyle() {
  ImGuiStyle &s = ImGui::GetStyle();

  s.Colors[ImGuiCol_WindowBg]        = ImVec4(0.08f, 0.05f, 0.03f, 0.92f);
  s.Colors[ImGuiCol_Border]          = ImVec4(0.45f, 0.30f, 0.10f, 1.00f);
  s.Colors[ImGuiCol_Text]            = kColorCream;
  s.Colors[ImGuiCol_TextDisabled]    = kColorMuted;
  s.Colors[ImGuiCol_TitleBg]         = ImVec4(0.10f, 0.06f, 0.02f, 1.00f);
  s.Colors[ImGuiCol_TitleBgActive]   = ImVec4(0.18f, 0.10f, 0.03f, 1.00f);
  s.Colors[ImGuiCol_Button]          = ImVec4(0.25f, 0.15f, 0.05f, 0.90f);
  s.Colors[ImGuiCol_ButtonHovered]   = ImVec4(0.40f, 0.25f, 0.08f, 1.00f);
  s.Colors[ImGuiCol_ButtonActive]    = ImVec4(0.55f, 0.35f, 0.10f, 1.00f);
  s.Colors[ImGuiCol_Separator]       = ImVec4(0.40f, 0.28f, 0.10f, 0.80f);
  s.Colors[ImGuiCol_FrameBg]         = ImVec4(0.12f, 0.08f, 0.03f, 0.80f);
  s.Colors[ImGuiCol_PopupBg]         = ImVec4(0.10f, 0.07f, 0.03f, 0.97f);

  s.WindowRounding   = 4.0f;
  s.FrameRounding    = 3.0f;
  s.PopupRounding    = 4.0f;
  s.WindowBorderSize = 1.5f;
  s.FrameBorderSize  = 0.0f;
  s.ItemSpacing      = ImVec2(8.0f, 5.0f);
  s.WindowPadding    = ImVec2(10.0f, 8.0f);
}

// ---------------------------------------------------------------------------
// Barra superior
// ---------------------------------------------------------------------------
void Hud::renderTopBar(AppState &state, const WaveState &ws) {
  using game_constants::kArcherCost;
  using game_constants::kArquebusCost;
  using game_constants::kCannonCost;
  using game_constants::kKnightCost;

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

    // ---- Slots de tropas ----
    const float slotW = 72.0f;
    float slotStartX = (state.fbWidth / 2.0f) - 2.0f * slotW;  // 4 slots centrados

    struct SlotDef { const char *id; unsigned int tex; int cost; int type; const char *nome; };
    const SlotDef slots[4] = {
      { "btn_archer",   textures_.archerIcon,   kArcherCost,   1, "Arqueiro"    },
      { "btn_arquebus", textures_.arquebusIcon,  kArquebusCost, 2, "Arcabuzeiro" },
      { "btn_cannon",   textures_.cannonIcon,    kCannonCost,   4, "Canhoneiro"  },
      { "btn_knight",   textures_.knightIcon,    kKnightCost,   3, "Comandante"  },
    };

    for (int i = 0; i < 4; i++) {
      float bx = slotStartX + i * slotW;
      bool selected  = state.isPlacingTroop && state.selectedTroopType == slots[i].type;
      bool canAfford = state.gold >= slots[i].cost;

      dl->AddRectFilled(ImVec2(bx + 4, 8), ImVec2(bx + slotW - 4, barH - 8),
                        IM_COL32(30, 20, 8, 180), 5.0f);
      if (selected)
        dl->AddRect(ImVec2(bx + 4, 8), ImVec2(bx + slotW - 4, barH - 8),
                    IM_COL32(220, 170, 30, 255), 5.0f, 0, 2.0f);

      ImGui::SetCursorPos(ImVec2(bx + (slotW - 44.0f) / 2.0f, (barH - 44.0f) / 2.0f - 4.0f));
      if (!canAfford) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.40f);
      if (ImGui::ImageButton(slots[i].id, (void *)(intptr_t)slots[i].tex, ImVec2(40, 40))) {
        if (canAfford) {
          state.isPlacingTroop    = true;
          state.selectedTroopType = slots[i].type;
          audio::playOneShot("data/audio/selection_sound.mp3");
        }
      }
      if (!canAfford) ImGui::PopStyleVar();

      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(kColorGold, "%s", slots[i].nome);
        ImGui::Text("Custo: %d GP", slots[i].cost);
        if (!canAfford) ImGui::TextColored(kColorRed, "Ouro insuficiente!");
        ImGui::EndTooltip();
      }

      // Custo embaixo
      char costStr[12];
      std::snprintf(costStr, sizeof(costStr), "%d GP", slots[i].cost);
      float tw = ImGui::CalcTextSize(costStr).x;
      ImGui::SetCursorPos(ImVec2(bx + (slotW - tw) / 2.0f, barH - 18.0f));
      ImGui::TextColored(canAfford ? kColorGold : kColorRed, "%s", costStr);
    }

  }
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
}

// ---------------------------------------------------------------------------
// Barra de feitiços — compra (canto inferior esquerdo)
// ---------------------------------------------------------------------------
// Espelha a barra de tropas, mas no canto inferior esquerdo e com ícones das
// formas geométricas (quadrado azul, triângulo laranja, círculo verde) em vez
// de retratos. Cada feitiço é consumível: clicar compra 1 carga (gasta ouro);
// desenhar a forma no modo de feitiço (F) gasta a carga. O índice "cls" é a
// classe da CNN: 0=círculo, 1=quadrado, 2=triângulo.
void Hud::renderSpellBar(AppState &state) {
  using game_constants::kSpellCircleCost;
  using game_constants::kSpellSquareCost;
  using game_constants::kSpellTriangleCost;

  const float slotW = 66.0f, slotH = 66.0f, gap = 6.0f;
  const float pad = 8.0f, titleH = 20.0f, costH = 18.0f;
  const float winW = 3.0f * slotW + 2.0f * gap + 2.0f * pad;
  const float winH = titleH + slotH + costH + 2.0f * pad;

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::SetNextWindowPos(ImVec2(10.0f, state.fbHeight - winH - 10.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.04f, 0.02f, 0.90f));

  // shape: 0=círculo, 1=quadrado, 2=triângulo. Desenha o ícone perfeito.
  auto drawIcon = [](ImDrawList *dl, int shape, ImVec2 c, float r, ImU32 col) {
    if (shape == 1) {
      dl->AddRectFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 2.0f);
    } else if (shape == 2) {
      ImVec2 v0(c.x, c.y - r);
      ImVec2 v1(c.x + 0.8660254f * r, c.y + 0.5f * r);
      ImVec2 v2(c.x - 0.8660254f * r, c.y + 0.5f * r);
      dl->AddTriangleFilled(v0, v1, v2, col);
    } else {
      dl->AddCircleFilled(c, r, col, 32);
    }
  };

  struct SpellSlot { int cls; int cost; const char *nome; };
  const SpellSlot slots[3] = {
      { 1, kSpellSquareCost,   "Lentid\xc3\xa3o" },  // quadrado azul
      { 2, kSpellTriangleCost, "Dano"           },   // triângulo laranja
      { 0, kSpellCircleCost,   "Veneno"         },   // círculo verde
  };

  if (ImGui::Begin("SpellBar", nullptr, flags)) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 win = ImGui::GetWindowPos();

    // Título
    ImGui::SetCursorPos(ImVec2(pad, 4.0f));
    ImGui::TextColored(kColorGold, "FEITI\xc3\x87OS");

    const float rowY = pad + titleH;
    for (int i = 0; i < 3; ++i) {
      const float bx = pad + i * (slotW + gap);
      const float by = rowY;
      const int   cls = slots[i].cls;
      const bool  canAfford = state.gold >= slots[i].cost;
      const int   charges   = state.spellCharges[cls];

      // Coordenadas absolutas (drawlist) do slot
      ImVec2 a(win.x + bx, win.y + by);
      ImVec2 b(win.x + bx + slotW, win.y + by + slotH);
      dl->AddRectFilled(a, b, IM_COL32(30, 20, 8, 200), 5.0f);
      dl->AddRect(a, b, IM_COL32(120, 85, 25, 200), 5.0f, 0, 1.5f);

      // Ícone da forma na cor do feitiço
      const float *rgb = spell::kSpellColors[cls];
      int alpha = canAfford ? 255 : 110;
      ImU32 col = IM_COL32(int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255), alpha);
      ImVec2 center(a.x + slotW * 0.5f, a.y + slotH * 0.5f);
      drawIcon(dl, cls, center, slotW * 0.30f, col);

      // Badge de cargas (canto superior direito do slot)
      if (charges > 0) {
        char cnt[8];
        std::snprintf(cnt, sizeof(cnt), "x%d", charges);
        ImVec2 ts = ImGui::CalcTextSize(cnt);
        ImVec2 tp(b.x - ts.x - 5.0f, a.y + 3.0f);
        dl->AddRectFilled(ImVec2(tp.x - 3, tp.y - 1), ImVec2(tp.x + ts.x + 3, tp.y + ts.y + 1),
                          IM_COL32(0, 0, 0, 180), 3.0f);
        dl->AddText(tp, IM_COL32(255, 230, 140, 255), cnt);
      }

      // Botão invisível por cima do slot para a compra
      ImGui::SetCursorPos(ImVec2(bx, by));
      if (ImGui::InvisibleButton(slots[i].nome, ImVec2(slotW, slotH))) {
        if (canAfford) {
          state.gold -= slots[i].cost;
          state.spellCharges[cls]++;
          audio::playOneShot("data/audio/selection_sound.mp3");
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(kColorGold, "%s", slots[i].nome);
        const char *desc = (cls == 1) ? "Inimigos na area: 75% da velocidade (permanente)"
                         : (cls == 2) ? "Inimigos na area perdem metade da vida"
                                      : "Inimigos na area: veneno 10%/s por 8s";
        ImGui::Text("%s", desc);
        ImGui::Text("Custo: %d GP  |  Cargas: %d", slots[i].cost, charges);
        if (!canAfford) ImGui::TextColored(kColorRed, "Ouro insuficiente!");
        ImGui::EndTooltip();
      }

      // Custo embaixo do slot
      char costStr[12];
      std::snprintf(costStr, sizeof(costStr), "%d GP", slots[i].cost);
      float tw = ImGui::CalcTextSize(costStr).x;
      ImGui::SetCursorPos(ImVec2(bx + (slotW - tw) * 0.5f, by + slotH + 1.0f));
      ImGui::TextColored(canAfford ? kColorGold : kColorRed, "%s", costStr);
    }
  }
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
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
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.03f, 0.01f, 0.88f));

  if (ImGui::Begin("WaveBar", nullptr, flags)) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const float W  = (float)state.fbWidth;

    // Borda inferior fina
    dl->AddLine(ImVec2(0, topBarH + barH - 1), ImVec2(W, topBarH + barH - 1),
                IM_COL32(120, 80, 20, 160), 1.0f);

    if (ws.phase == WavePhase::Victory) {
      const char *v = "  VICT\xc3\x93RIA!  Todas as ondas foram derrotadas.";
      ImGui::SetCursorPos(ImVec2((W - ImGui::CalcTextSize(v).x) * 0.5f, 7.0f));
      ImGui::TextColored(kColorGold, "%s", v);
    } else {
      const WaveDef &def = kWaves[ws.currentWave];

      // Rótulo da onda (esquerda)
      char lbl[24];
      std::snprintf(lbl, sizeof(lbl), "  ONDA %d / 10", ws.currentWave + 1);
      const float lblW = 110.0f;
      ImGui::SetCursorPos(ImVec2(8.0f, 8.0f));
      ImGui::TextColored(kColorGold, "%s", lbl);

      // Texto de fase (direita)
      char rhs[40];
      if (ws.phase == WavePhase::Intermission) {
        int secs = static_cast<int>(ws.timer) + 1;
        std::snprintf(rhs, sizeof(rhs), "Em %ds  \xe2\x80\x94  [Y] Pular  ", secs);
      } else if (ws.phase == WavePhase::Starting) {
        std::snprintf(rhs, sizeof(rhs), "  INICIANDO...  ");
      } else {
        std::snprintf(rhs, sizeof(rhs), "Mortos: %d / %d  ",
                      ws.killedCount, def.enemyCount);
      }
      float rhsW = ImGui::CalcTextSize(rhs).x;
      ImGui::SetCursorPos(ImVec2(W - rhsW - 6.0f, 8.0f));
      ImVec4 rhsColor = (ws.phase == WavePhase::Intermission) ? kColorMuted
                      : (ws.phase == WavePhase::Starting)      ? kColorGold
                                                               : kColorGreen;
      ImGui::TextColored(rhsColor, "%s", rhs);

      // Barra de progresso (centro, toma o espaço restante)
      const float barPad = 8.0f;
      const float bx     = lblW + barPad;
      const float bw     = W - lblW - rhsW - barPad * 2.0f - 12.0f;
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

    // ---- Seção central: ícone + stats ----
    const float iconSz = 40.0f;
    const float iconX  = leftW + 10.0f;
    const float iconY  = (bannerH - iconSz) * 0.5f;

    // Fundo e borda do ícone
    ImVec2 icSS = ImVec2(iconX, bannerY + iconY);
    dl->AddRectFilled(icSS, ImVec2(icSS.x + iconSz, icSS.y + iconSz),
                      IM_COL32(15, 10, 4, 220), 4.0f);
    dl->AddRect(icSS, ImVec2(icSS.x + iconSz, icSS.y + iconSz),
                IM_COL32(130, 88, 18, 200), 4.0f, 0, 1.0f);

    ImGui::SetCursorPos(ImVec2(iconX, iconY));
    ImGui::Image((void *)(intptr_t)textures_.zombiePortrait, ImVec2(iconSz, iconSz));
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::TextColored(kColorGold, "Zumbi");
      ImGui::Separator();
      ImGui::Text("Quantidade : %d", def.enemyCount);
      ImGui::Text("Vida       : %d", def.enemyStats.maxHp);
      ImGui::Text("Velocidade : %.1f", def.enemyStats.speed);
      ImGui::Text("Dano       : %d", def.enemyStats.damage);
      ImGui::EndTooltip();
    }

    const float statsX = iconX + iconSz + 10.0f;
    ImGui::SetCursorPos(ImVec2(statsX, yTop));
    ImGui::TextColored(kColorCream, "Zumbis");
    ImGui::SameLine(0, 6);
    ImGui::TextColored(kColorGold, "\xc3\x97 %d", def.enemyCount);

    ImGui::SetCursorPos(ImVec2(statsX, yBot));
    ImGui::TextColored(kColorMuted, "HP %d", def.enemyStats.maxHp);
    ImGui::SameLine(0, 14);
    ImGui::TextColored(kColorMuted, "Vel %.1f", def.enemyStats.speed);
    ImGui::SameLine(0, 14);
    ImGui::TextColored(kColorMuted, "Dano %d", def.enemyStats.damage);

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
void Hud::renderDebugWindow(const AppState &state, float fps) {
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
  }
  ImGui::End();
}

// ---------------------------------------------------------------------------
void Hud::render(AppState &state, const WaveState &ws, float fps) {
  renderTopBar(state, ws);
  renderSpellBar(state);
  renderWaveBar(state, ws);
  renderIntermissionOverlay(ws);
  if (ws.phase == WavePhase::Victory) renderVictoryOverlay();
  renderDebugWindow(state, fps);
}

void Hud::shutdown() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
