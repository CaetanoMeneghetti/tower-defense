#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "game/enemy_system.h"

// =============================================================================
// SISTEMA DE FLECHAS
// =============================================================================

constexpr float kArrowSpeed = 22.0f;
constexpr float kArrowScale = 0.12f;

struct ArrowProjectile {
  glm::vec3 position;
  glm::vec3 velocity;      // atualizado todo frame para seguir o alvo
  float     traveled  = 0.0f;
  float     maxDist   = 0.0f;
  int       damage    = 0;
  int       targetIdx = -1;  // índice do inimigo alvo — sempre acerta
};

// Avança todas as flechas em direção ao alvo (homing), aplica dano ao chegar
// perto e remove flechas que acertaram ou passaram de maxDist.
void updateArrows(std::vector<ArrowProjectile>      &arrows,
                  std::vector<EnemyInstance>         &enemies,
                  const std::vector<EnemyTickResult> &ticks,
                  float                               deltaTime);
