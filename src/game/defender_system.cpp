#include "game/defender_system.h"

#include <cmath>

#include "game/collisions.h"
#include "game/game_constants.h"

// Retorna o índice do inimigo mais avançado no path (e vivo) ou -1.
static int findBestTarget(const std::vector<EnemyInstance> &enemies) {
  int   best = -1;
  float maxD = -1.0f;
  for (int i = 0; i < (int)enemies.size(); ++i) {
    if (enemies[i].alive && enemies[i].pathDistance > maxD) {
      maxD = enemies[i].pathDistance;
      best = i;
    }
  }
  return best;
}

DefenderFireResult updateDefenders(std::vector<GameObject> &defenders,
                                   std::vector<DefenderShoot> &defenderShoots,
                                   std::vector<EnemyInstance> &enemies,
                                   const std::vector<EnemyTickResult> &enemyTicks,
                                   float deltaTime) {
  DefenderFireResult result;

  if (defenderShoots.size() < defenders.size())
    defenderShoots.resize(defenders.size(), DefenderShoot{});

  for (size_t i = 0; i < defenders.size(); ++i) {
    auto &unit  = defenders[i];
    auto &shoot = defenderShoots[i];

    // ---- Arqueiro ----
    if (unit.type == defender_types::kArcher) {
      const float range    = game_constants::kArcherRange;
      const float interval = game_constants::kArcherShootInterval;
      const int   dmg      = game_constants::kArcherArrowDamage;

      int tidx = findBestTarget(enemies);
      bool canShoot = false;
      float dx = 0.f, dz = 0.f;
      if (tidx >= 0) {
        dx = enemyTicks[tidx].position[0] - unit.position[0];
        dz = enemyTicks[tidx].position[2] - unit.position[2];
        canShoot = collisions::inRange(unit.position[0], unit.position[2],
                                        enemyTicks[tidx].position[0], enemyTicks[tidx].position[2],
                                        range);
      }

      if (canShoot) {
        unit.rotationY = -std::atan2(dx, dz);
        if (!shoot.aiming) {
          unit.setAnimation("aim");
          shoot.aiming     = true;
          shoot.shootTimer = 0.0f;
        }
        shoot.shootTimer += deltaTime;
        if (shoot.shootTimer >= interval) {
          shoot.shootTimer -= interval;
          enemies[tidx].hp -= dmg;
          enemies[tidx].hitFlashTime = game_constants::kEnemyHitFlashDuration;
          if (enemies[tidx].hp <= 0) enemies[tidx].hp = 0;
          ++result.archerFired;
        }
      } else if (shoot.aiming) {
        shoot.aiming     = false;
        shoot.shootTimer = 0.0f;
        unit.setIdleAnimations({"idle1"});
      }
    }

    // ---- Arcabuz: fases fire → reload ----
    if (unit.type == defender_types::kArquebus) {
      const float range  = game_constants::kArquebusRange;
      const int   dmg    = game_constants::kArquebusBulletDamage;
      const float fireDur   = game_constants::kArquebusFireDuration;
      const float reloadDur = game_constants::kArquebusReloadDuration;

      int tidx = findBestTarget(enemies);
      bool enemyInRange = false;
      float dx = 0.f, dz = 0.f;
      if (tidx >= 0) {
        dx = enemyTicks[tidx].position[0] - unit.position[0];
        dz = enemyTicks[tidx].position[2] - unit.position[2];
        enemyInRange = collisions::inRange(unit.position[0], unit.position[2],
                                            enemyTicks[tidx].position[0], enemyTicks[tidx].position[2],
                                            range);
      }

      if (shoot.reloading) {
        // Conclui o reload independente do alvo
        shoot.shootTimer += deltaTime;
        if (shoot.shootTimer >= reloadDur) {
          shoot.reloading  = false;
          shoot.aiming     = false;
          shoot.shootTimer = 0.0f;
          unit.setIdleAnimations({"idle1"});
        }
      } else if (shoot.aiming) {
        // Conclui a animação de disparo
        shoot.shootTimer += deltaTime;
        if (shoot.shootTimer >= fireDur) {
          shoot.aiming     = false;
          shoot.reloading  = true;
          shoot.shootTimer = 0.0f;
          unit.setAnimationReverse("reload", game_constants::kArquebusReloadDuration);
        }
      } else if (enemyInRange) {
        // Início do ciclo: dispara imediatamente (som + dano no frame da animação)
        unit.rotationY = -std::atan2(dx, dz);
        unit.setAnimation("fire");
        shoot.aiming     = true;
        shoot.reloading  = false;
        shoot.shootTimer = 0.0f;
        enemies[tidx].hp -= dmg;
        enemies[tidx].hitFlashTime = game_constants::kEnemyHitFlashDuration;
        if (enemies[tidx].hp <= 0) enemies[tidx].hp = 0;
        ++result.arquebusFired;
      } else if (!shoot.aiming && !shoot.reloading) {
        // Sem alvo e idle — garante animação idle
        // (só redefine se estava saindo de outra animação)
      }

      // Se saiu do range enquanto idle (não no meio de um ciclo), atualiza rotação
      if (!shoot.aiming && !shoot.reloading && enemyInRange && tidx >= 0)
        unit.rotationY = -std::atan2(dx, dz);
    }

    unit.update(deltaTime);
  }
  return result;
}
