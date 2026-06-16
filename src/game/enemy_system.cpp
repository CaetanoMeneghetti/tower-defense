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
  clearSpellEffects(enemy);
  return enemy;
}

void clearSpellEffects(EnemyInstance &enemy) {
  enemy.speedMultiplier = 1.0f;
  enemy.poisonTimer     = 0.0f;
  enemy.poisonAccum     = 0.0f;
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

  // --- Morte por HP (gate em alive evita re-trigger com hp<=0 persistente) ---
  if (enemy.alive && enemy.hp <= 0) {
    enemy.alive          = false;
    enemy.respawnTimer   = game_constants::kEnemyRespawnDelay;
    result.diedThisFrame = true;
    enemyModel.setAnimation("death", false);
  }

  // --- Veneno do feitiço círculo (dano contínuo) ---
  // Tira kSpellPoisonFraction (10%) da vida máx por segundo enquanto poisonTimer
  // > 0. Como hp é int, acumula o fracionário em poisonAccum e desconta o inteiro.
  if (enemy.alive && enemy.poisonTimer > 0.0f) {
    enemy.poisonTimer -= deltaTime;
    if (enemy.poisonTimer < 0.0f) enemy.poisonTimer = 0.0f;
    enemy.poisonAccum +=
        enemy.stats.maxHp * game_constants::kSpellPoisonFraction * deltaTime;
    int whole = static_cast<int>(enemy.poisonAccum);
    if (whole > 0) {
      enemy.hp -= whole;
      enemy.poisonAccum -= static_cast<float>(whole);
      enemy.hitFlashTime = game_constants::kEnemyHitFlashDuration;
    }
  }

  // --- Movimento ou animação de morte ---
  if (enemy.alive) {
    // speedMultiplier carrega a lentidão permanente do feitiço quadrado (0.75).
    enemy.pathDistance += enemy.stats.speed * enemy.speedMultiplier * deltaTime;
    enemyModel.update(deltaTime);
  } else {
    enemyModel.update(deltaTime);

    // O timer sempre conta (deixa a morte animar); waveControlled só suprime o
    // respawn automático ao fim dele.
    if (enemy.respawnTimer > 0.0f) {
      enemy.respawnTimer -= deltaTime;
      if (enemy.respawnTimer < 0.0f) enemy.respawnTimer = 0.0f;
    }
    if (enemy.respawnTimer <= 0.0f && !enemy.waveControlled) {
      enemy.hp           = enemy.stats.maxHp;
      enemy.pathDistance = 0.0f;
      enemy.alive        = true;
      enemy.hitFlashTime = 0.0f;
      clearSpellEffects(enemy);
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
    enemyModel.setAnimation("death", false);
  }

  return result;
}
