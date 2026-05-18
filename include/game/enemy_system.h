#pragma once

#include <vector>

#include "engine/catmull_rom.h"
#include "engine/game_object.h"
#include "game/app_state.h"
#include "game/path_navigation.h"
#include "math/vector.h"

// =============================================================================
// INIMIGOS — STATS + ESTADO RUNTIME + UPDATE
// =============================================================================
// Stats por tipo (HP, velocidade no path, dano ao castelo). Para criar um novo
// inimigo, basta declarar outra EnemyStats e instanciar uma EnemyInstance.

struct EnemyStats {
  int maxHp;
  float speed;
  int damage;
};

struct EnemyInstance {
  EnemyStats stats;
  int hp;
  float pathDistance;
  bool alive;
  float respawnTimer;
  // Tempo restante de flash vermelho ao receber hit.
  float hitFlashTime;
};

// Stats pré-definidas (declaração em .cpp).
extern const EnemyStats kZombieStats;

// Inicializa uma instância no início do path.
EnemyInstance makeEnemy(const EnemyStats &stats);

// Resultado do tick do inimigo: posição atual + ângulo + se acabou de chegar.
struct EnemyTickResult {
  Vector<3> position;
  float angle;
  bool reachedEnd;
};

// Avança o inimigo no path, decrementa respawn timer e atualiza flash.
// Caso o inimigo alcance o castelo, decrementa state.health e respawna.
EnemyTickResult updateEnemy(EnemyInstance &enemy,
                            GameObject &enemyModel,
                            const std::vector<Point> &curvePoints,
                            const PathCache &curveCache,
                            AppState &state,
                            float deltaTime);
