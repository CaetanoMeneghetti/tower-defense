#pragma once

#include <GLFW/glfw3.h>

#include "game/app_state.h"
#include "game/wave_system.h"

// =============================================================================
// HUD (ImGui) — barra superior + overlay de wave + debug
// =============================================================================

struct HudTextures {
  unsigned int topBackground;
  unsigned int goldIcon;
  unsigned int healthIcon;
  unsigned int archerIcon;
  unsigned int arquebusIcon;
  unsigned int cannonIcon;
  unsigned int knightIcon;
  unsigned int zombiePortrait;
  unsigned int armoredZombiePortrait;
  unsigned int chargerPortrait;
  unsigned int necromancerPortrait;
  unsigned int archerUpIcon[4];    // índice 0=nível 1 … 3=nível 4
  unsigned int arcabuzUpIcon[4];
  unsigned int cannonUpIcon[4];
  unsigned int commanderUpIcon[4];
};

class Hud {
 public:
  Hud();
  ~Hud();

  void init(GLFWwindow *window);
  void setTextures(const HudTextures &textures);

  // state é lido/escrito (gold, placement). ws e fps são somente leitura.
  void render(AppState &state, const WaveState &ws, float fps);

  void shutdown();

 private:
  void setupStyle();
  void renderTopBar(AppState &state, const WaveState &ws);
  // Menu de compra estilo "loja" (tecla M): abas Aliados/Feitiços à esquerda,
  // cards das opções e painel de detalhe com botão COMPRAR.
  void renderBuyMenu(AppState &state);
  // Legenda de atalhos sempre visível (canto inferior esquerdo).
  void renderControlsLegend(const AppState &state);
  void renderWaveBar(const AppState &state, const WaveState &ws);
  void renderIntermissionOverlay(const WaveState &ws);
  void renderVictoryOverlay();
  void renderDebugWindow(const AppState &state, float fps);

  HudTextures textures_;
};
