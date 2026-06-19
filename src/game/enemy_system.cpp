#include "game/enemy_system.h"

#include "game/game_constants.h"

const EnemyStats kZombieStats     { 50, 2.0f, 20, enemy_types::kNormal      };
const EnemyStats kChargerStats    { 80, 2.2f, 25, enemy_types::kCharger     };
const EnemyStats kNecromancerStats{ 60, 1.6f, 15, enemy_types::kNecromancer };

constexpr float kChargerRageSpeedMult  = 2.5f;
constexpr float kChargerScreamDur      = 1.6f;  // duração aproximada da animação de scream
constexpr float kNecromancerSummonDur  = 2.2f;   // duração da animação de invocação
constexpr float kNecromancerCooldownMin = 7.0f;
constexpr float kNecromancerCooldownMax = 12.0f;

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
  result.angle          = 0.0f;
  result.reachedEnd     = false;
  result.diedThisFrame  = false;
  result.wantsToSummon  = false;

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

  // --- Charger: scream ao atingir 50% HP, depois corre rápido ---
  if (enemy.alive && enemy.stats.type == enemy_types::kCharger) {
    if (!enemy.enraged && enemy.summonAnimTimer > 0.0f) {
      // Está no meio do scream — conta o tempo
      enemy.summonAnimTimer -= deltaTime;
      if (enemy.summonAnimTimer <= 0.0f) {
        enemy.enraged = true;
        enemy.stats.speed *= kChargerRageSpeedMult;
        enemyModel.setAnimation("run");
      }
    } else if (!enemy.enraged && enemy.summonAnimTimer <= 0.0f) {
      if (enemy.hp <= enemy.stats.maxHp / 2) {
        enemy.summonAnimTimer = kChargerScreamDur;
        enemyModel.setAnimation("scream", false);
      }
    }
  }

  // --- Necromancer: ciclo de invocação ---
  if (enemy.alive && enemy.stats.type == enemy_types::kNecromancer) {
    if (enemy.isSummoning) {
      enemy.summonAnimTimer -= deltaTime;
      if (enemy.summonAnimTimer <= 0.0f) {
        enemy.isSummoning    = false;
        enemy.summonCooldown = kNecromancerCooldownMin +
            std::fmod(enemy.pathDistance * 13.7f, kNecromancerCooldownMax - kNecromancerCooldownMin);
        result.wantsToSummon = true;
        enemyModel.setAnimation("walk");
      }
    } else {
      enemy.summonCooldown -= deltaTime;
      if (enemy.summonCooldown <= 0.0f) {
        enemy.isSummoning   = true;
        enemy.summonAnimTimer = kNecromancerSummonDur;
        enemyModel.setAnimation("summon", false);
      }
    }
  }

  // --- Movimento ou animação de morte ---
  if (enemy.alive) {
    // Charger pausa durante o scream; necromancer pausa durante a invocação.
    const bool chargerScreaming = (enemy.stats.type == enemy_types::kCharger
                                   && !enemy.enraged && enemy.summonAnimTimer > 0.0f);
    const bool necroSummoning   = (enemy.stats.type == enemy_types::kNecromancer
                                   && enemy.isSummoning);
    if (!chargerScreaming && !necroSummoning)
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
      enemy.hp             = enemy.stats.maxHp;
      enemy.pathDistance   = 0.0f;
      enemy.alive          = true;
      enemy.hitFlashTime   = 0.0f;
      enemy.enraged        = false;
      enemy.isSummoning    = false;
      enemy.summonCooldown = kNecromancerCooldownMin;
      clearSpellEffects(enemy);
      if (enemy.stats.type == enemy_types::kNecromancer)
        enemyModel.setAnimation("walk");
      else
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
