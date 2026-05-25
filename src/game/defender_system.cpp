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
      const float range    = unit.range;
      const float interval = unit.fireRate;
      const int   dmg      = unit.damage;

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
      const float range     = unit.range;
      const int   dmg       = unit.damage;
      const float fireDur   = game_constants::kArquebusFireDuration;
      const float reloadDur = unit.fireRate;

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
          unit.setAnimationReverse("reload", reloadDur);
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
        {
          float fwdX = -std::sin(unit.rotationY);
          float fwdZ =  std::cos(unit.rotationY);
          result.arquebusShotPositions.push_back(glm::vec3(
              unit.position[0] + fwdX * 1.2f,
              unit.position[1] + 1.2f,
              unit.position[2] + fwdZ * 1.2f));
        }
      } else if (!shoot.aiming && !shoot.reloading) {
        // Sem alvo e idle — garante animação idle
        // (só redefine se estava saindo de outra animação)
      }

      // Se saiu do range enquanto idle (não no meio de um ciclo), atualiza rotação
      if (!shoot.aiming && !shoot.reloading && enemyInRange && tidx >= 0)
        unit.rotationY = -std::atan2(dx, dz);
    }

    // ---- Canhão: idle → fire → cower → idle ----
    if (unit.type == defender_types::kCannon) {
      const float range    = unit.range;
      const int   dmg      = unit.damage;
      const float fireDur  = game_constants::kCannonFireDuration;
      const float cowerDur = game_constants::kCannonCowerDuration;

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
        // Fase 3 (cower) e fase 4 (cower reverso).
        shoot.shootTimer += deltaTime;
        if (shoot.shootTimer >= cowerDur) {
          shoot.shootTimer = 0.0f;
          if (!shoot.reversing) {
            // Tiro acontece aqui — inicio do cower reverso.
            shoot.reversing = true;
            unit.setAnimationReverse("cower", cowerDur);
            ++result.cannonFired;
            // Atualiza orientação para o alvo atual e registra posição do disparo.
            if (tidx >= 0) {
              unit.rotationY = -std::atan2(dx, dz);
              enemies[tidx].hp -= dmg;
              enemies[tidx].hitFlashTime = game_constants::kEnemyHitFlashDuration;
              if (enemies[tidx].hp <= 0) enemies[tidx].hp = 0;
            }
            {
              using game_constants::kCannonBarrelOffset;
              float fwdX = -std::sin(unit.rotationY);
              float fwdZ =  std::cos(unit.rotationY);
              const float muzzleDist = kCannonBarrelOffset + 1.5f;
              result.cannonShotPositions.push_back(glm::vec3(
                  unit.position[0] + fwdX * muzzleDist,
                  unit.position[1] + 0.35f,
                  unit.position[2] + fwdZ * muzzleDist));
            }
          } else {
            shoot.reloading = false;
            shoot.reversing = false;
            unit.setIdleAnimations({"idle1"});
          }
        }
      } else if (shoot.aiming) {
        // Fase 1 (fire) e fase 2 (fire reverso).
        shoot.shootTimer += deltaTime;
        if (shoot.shootTimer >= fireDur) {
          shoot.shootTimer = 0.0f;
          if (!shoot.reversing) {
            shoot.reversing = true;
            unit.setAnimationReverse("fire", fireDur);
          } else {
            shoot.aiming    = false;
            shoot.reloading = true;
            shoot.reversing = false;
            unit.setAnimation("cower");
          }
        }
      } else if (enemyInRange) {
        // Inicia ciclo: orienta o canhoneiro e começa animação de fire.
        unit.rotationY = -std::atan2(dx, dz);
        unit.setAnimation("fire");
        shoot.aiming     = true;
        shoot.reloading  = false;
        shoot.reversing  = false;
        shoot.shootTimer = 0.0f;
      }
    }

    // ---- Comandante de Cavaleiros ----
    if (unit.type == defender_types::kKnight) {
      const float interval = game_constants::kKnightCommandInterval;
      const float animDur  = game_constants::kKnightCommandAnimDuration;

      if (shoot.aiming) {
        // Reproduzindo animação de comando
        shoot.shootTimer += deltaTime;
        if (shoot.shootTimer >= animDur) {
          shoot.aiming     = false;
          shoot.shootTimer = 0.0f;
          unit.setIdleAnimations({"knightIdle"});
        }
      } else {
        shoot.shootTimer += deltaTime;
        if (shoot.shootTimer >= interval) {
          shoot.shootTimer = 0.0f;
          shoot.aiming     = true;
          unit.setAnimation("command");
          result.knightSummoned += (unit.damage > 0) ? unit.damage : 1;
          ++result.chargePlayed;
        }
      }
    }

    unit.update(deltaTime);
  }
  return result;
}
