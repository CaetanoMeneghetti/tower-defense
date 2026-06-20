#pragma once

#include <vector>

#include "engine/game_object.h"
#include "game/app_state.h"
#include "game/enemy_system.h"
#include "game/path_navigation.h"
#include "math/vector.h"

struct KnightInstance {
  int hp;
  int maxHp;
  float speed;
  int contactDamage;        // dano que o knight causa ao colidir
  float slashInterval;      // segundos entre slashes
  float slashTimer;
  float pathDistance;       // distância do knight no path (começa em totalDistance e diminui)
  float collisionRadius;    // raio da bbox circular
  bool alive;
  bool slashing;
  bool dying = false;
  float deathTimer = 0.0f;
  int weaponLevel = 1;      // nível de armas (herdado do comandante que invocou)
};

KnightInstance makeKnight(const PathCache &curveCache);

// Retorna posição e ângulo do knight no path (igual EnemyTickResult).
struct KnightTickResult {
  Vector<3> position;
  float angle;
  bool hitThisFrame = false;  // true quando o slash causou dano neste frame
};

KnightTickResult updateKnight(KnightInstance &knight,
                              GameObject &knightModel,
                              std::vector<EnemyInstance> &enemies,
                              const std::vector<EnemyTickResult> &enemyTicks,
                              const std::vector<Point> &curvePoints,
                              const PathCache &curveCache,
                              float deltaTime);