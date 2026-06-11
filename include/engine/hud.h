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
  unsigned int zombiePortrait;  // exibido no overlay de intermission
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
  void renderSpellBar(AppState &state);
  void renderWaveBar(const AppState &state, const WaveState &ws);
  void renderIntermissionOverlay(const WaveState &ws);
  void renderVictoryOverlay();
  void renderDebugWindow(const AppState &state, float fps);

  HudTextures textures_;
};
