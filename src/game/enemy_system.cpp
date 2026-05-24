#include "game/enemy_system.h"

#include "game/game_constants.h"

const EnemyStats kZombieStats{50, 2.0f, 20};

EnemyInstance makeEnemy(const EnemyStats &stats) {
  EnemyInstance enemy;
  enemy.stats          = stats;
  enemy.hp             = stats.maxHp;
  enemy.pathDistance   = 0.0f;
  enemy.alive          = true;
  enemy.respawnTimer   = 0.0f;
  enemy.hitFlashTime   = 0.0f;
  enemy.waveControlled = false;
  return enemy;
}

EnemyTickResult updateEnemy(EnemyInstance &enemy,
                            GameObject &enemyModel,
                            const std::vector<Point> &curvePoints,
                            const PathCache &curveCache,
                            AppState &state,
                            float deltaTime) {
  EnemyTickResult result;
  result.angle         = 0.0f;
  result.reachedEnd    = false;
  result.diedThisFrame = false;

  // --- Morte por HP (só dispara enquanto alive; evita re-trigger com hp<=0 persistente) ---
  if (enemy.alive && enemy.hp <= 0) {
    enemy.alive          = false;
    enemy.respawnTimer   = game_constants::kEnemyRespawnDelay;
    result.diedThisFrame = true;
    enemyModel.setAnimation("death");
  }

  // --- Movimento ou animação de morte ---
  if (enemy.alive) {
    enemy.pathDistance += enemy.stats.speed * deltaTime;
    enemyModel.update(deltaTime);
  } else {
    enemyModel.update(deltaTime);

    // O timer sempre conta — serve para a animação de morte terminar.
    // waveControlled só suprime o respawn automático ao fim do timer.
    if (enemy.respawnTimer > 0.0f) {
      enemy.respawnTimer -= deltaTime;
      if (enemy.respawnTimer < 0.0f) enemy.respawnTimer = 0.0f;
    }
    if (enemy.respawnTimer <= 0.0f && !enemy.waveControlled) {
      enemy.hp           = enemy.stats.maxHp;
      enemy.pathDistance = 0.0f;
      enemy.alive        = true;
      enemy.hitFlashTime = 0.0f;
      enemyModel.setAnimation("run");
    }
  }

  // --- Flash de hit ---
  if (enemy.hitFlashTime > 0.0f) {
    enemy.hitFlashTime -= deltaTime;
    if (enemy.hitFlashTime < 0.0f) enemy.hitFlashTime = 0.0f;
  }

  // --- Posição no path ---
  result.position = getPositionAtDistance(
      curvePoints, curveCache, enemy.pathDistance, result.angle, result.reachedEnd);

  // --- Chegou ao castelo ---
  if (enemy.alive && result.reachedEnd) {
    state.health -= enemy.stats.damage;
    if (state.health < 0) state.health = 0;
    enemy.alive          = false;
    enemy.respawnTimer   = game_constants::kEnemyRespawnDelay;
    result.diedThisFrame = true;
    enemyModel.setAnimation("death");
  }

  return result;
}
