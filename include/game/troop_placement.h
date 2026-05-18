#pragma once

#include <GLFW/glfw3.h>

#include <vector>

#include "engine/camera.h"
#include "engine/catmull_rom.h"
#include "engine/game_object.h"
#include "game/app_state.h"
#include "game/defender_system.h"
#include "math/vector.h"

// =============================================================================
// PLACEMENT DE TROPAS (preview + clique)
// =============================================================================
// Quando state.isPlacingTroop é true:
//  - desenha um "fantasma" colorido (azul válido / vermelho inválido) no chão;
//  - aceita clique esquerdo se a posição for válida (gera um defender novo);
//  - cancela com clique direito.

struct TroopPlacementContext {
  GLFWwindow *window;
  const Camera &cam;
  const Vector<3> &cameraPosition;
  const std::vector<Point> &curvePoints;
  unsigned int previewShader;
  const TroopDef &archerClass;
  const TroopDef &arquebusClass;
  std::vector<GameObject> &defenders;
  std::vector<DefenderShoot> &defenderShoots;
  const float *glView;
  const float *glProj;
};

void handleTroopPlacement(const TroopPlacementContext &ctx, AppState &state, float deltaTime);
