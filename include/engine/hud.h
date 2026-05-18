#pragma once

#include <GLFW/glfw3.h>

#include "game/app_state.h"

// =============================================================================
// HUD (ImGui) — barra superior + janela de debug
// =============================================================================

struct HudTextures {
  unsigned int topBackground;
  unsigned int goldIcon;
  unsigned int healthIcon;
  unsigned int archerIcon;
  unsigned int arquebusIcon;
};

class Hud {
 public:
  Hud();
  ~Hud();

  void init(GLFWwindow *window);
  void setTextures(const HudTextures &textures);
  void render(AppState &state, float fps);
  void shutdown();

 private:
  void setupStyle();
  HudTextures textures_;
};
