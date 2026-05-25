#pragma once

#include <vector>

#include "engine/game_object.h"
#include "game/enemy_system.h"
#include "math/vector.h"

// =============================================================================
// DEFENSORES — ESTADO DE TIRO + UPDATE
// =============================================================================

struct DefenderShoot {
  float shootTimer = 0.0f;
  bool  aiming     = false;   // arqueiro: em animação de mira/disparo
  bool  reloading  = false;   // arcabuz: fase de reload após disparo
  bool  reversing  = false;   // canhão: sub-fase reversa (fire_rev ou cower_rev)
};

namespace defender_types {
constexpr int kNone     = 0;
constexpr int kArcher   = 1;
constexpr int kArquebus = 2;
constexpr int kKnight   = 3;
constexpr int kCannon   = 4;
}  // namespace defender_types

struct DefenderFireResult {
  int archerFired    = 0;
  int arquebusFired  = 0;
  int cannonFired    = 0;
  int knightSummoned = 0;
  int chargePlayed   = 0;
  std::vector<glm::vec3> cannonShotPositions;
  std::vector<glm::vec3> arquebusShotPositions;
};

// enemies     = todos os slots de inimigos (vivos e mortos)
// enemyTicks  = posições calculadas no frame atual (alinhado com enemies)
DefenderFireResult updateDefenders(std::vector<GameObject> &defenders,
                                   std::vector<DefenderShoot> &defenderShoots,
                                   std::vector<EnemyInstance> &enemies,
                                   const std::vector<EnemyTickResult> &enemyTicks,
                                   float deltaTime);
