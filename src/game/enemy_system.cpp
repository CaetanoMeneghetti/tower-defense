#include "game/enemy_system.h"

#include "game/game_constants.h"

const EnemyStats kZombieStats{50, 2.0f, 20};

EnemyInstance makeEnemy(const EnemyStats &stats) {
  EnemyInstance enemy;
  enemy.stats = stats;
  enemy.hp = enemy.stats.maxHp;
  enemy.pathDistance = 0.0f;
  enemy.alive = true;
  enemy.respawnTimer = 0.0f;
  enemy.hitFlashTime = 0.0f;
  return enemy;
}

EnemyTickResult updateEnemy(EnemyInstance &enemy,
                            GameObject &enemyModel,
                            const std::vector<Point> &curvePoints,
                            const PathCache &curveCache,
                            AppState &state,
                            float deltaTime) {
  // --- Movimento ou respawn ---
  if (enemy.alive) {
    enemy.pathDistance += enemy.stats.speed * deltaTime;
    enemyModel.update(deltaTime);
  } else {
    enemy.respawnTimer -= deltaTime;
    if (enemy.respawnTimer <= 0.0f) {
      enemy.hp = enemy.stats.maxHp;
      enemy.pathDistance = 0.0f;
      enemy.alive = true;
      enemy.hitFlashTime = 0.0f;
    }
  }

  // --- Flash de hit ---
  if (enemy.hitFlashTime > 0.0f) {
    enemy.hitFlashTime -= deltaTime;
    if (enemy.hitFlashTime < 0.0f) enemy.hitFlashTime = 0.0f;
  }

  // --- Posição atual no path ---
  EnemyTickResult result;
  result.angle = 0.0f;
  result.reachedEnd = false;
  result.position = getPositionAtDistance(
      curvePoints, curveCache, enemy.pathDistance, result.angle, result.reachedEnd);

  // --- Chegou no castelo? ---
  if (enemy.alive && result.reachedEnd) {
    state.health -= enemy.stats.damage;
    if (state.health < 0) state.health = 0;
    enemy.alive = false;
    enemy.respawnTimer = game_constants::kEnemyRespawnDelay;
  }

  return result;
}
