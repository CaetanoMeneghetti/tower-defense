#include "game/defender_system.h"

#include <cmath>

#include "game/collisions.h"
#include "game/combat.h"
#include "game/game_constants.h"

// Índice do inimigo vivo mais avançado no path dentro de `range` de (ox,oz), ou
// -1. Os inimigos são slots não ordenados, então varremos todos pelo maior
// pathDistance no raio.
static int findBestTargetInRange(const std::vector<EnemyInstance> &enemies,
                                 const std::vector<EnemyTickResult> &ticks,
                                 float ox, float oz, float range) {
  int   best = -1;
  float maxD = -1.0f;
  for (int i = 0; i < (int)enemies.size(); ++i) {
    if (!enemies[i].alive) continue;
    if (!collisions::inRange(ox, oz, ticks[i].position[0], ticks[i].position[2], range))
      continue;
    if (enemies[i].pathDistance > maxD) {
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

  // Comandante só invoca com inimigos em campo (evita invocar na intermissão).
  bool anyEnemyAlive = false;
  for (const auto &e : enemies)
    if (e.alive) { anyEnemyAlive = true; break; }

  for (size_t i = 0; i < defenders.size(); ++i) {
    auto &unit  = defenders[i];
    auto &shoot = defenderShoots[i];

    // ---- Arqueiro ----
    if (unit.type == defender_types::kArcher) {
      const float range    = unit.range;
      const float interval = unit.fireRate;
      const int   dmg      = unit.damage;

      int tidx = findBestTargetInRange(enemies, enemyTicks,
                                       unit.position[0], unit.position[2], range);
      bool canShoot = (tidx >= 0);
      float dx = 0.f, dz = 0.f;
      if (canShoot) {
        dx = enemyTicks[tidx].position[0] - unit.position[0];
        dz = enemyTicks[tidx].position[2] - unit.position[2];
      }

      if (canShoot) {
        unit.rotationY = -std::atan2(dx, dz);
        if (!shoot.aiming) {
          unit.setAnimation("aim", false);  // segura a corda esticada
          shoot.aiming     = true;
          shoot.shootTimer = 0.0f;
        }
        shoot.shootTimer += deltaTime;
        if (shoot.shootTimer >= interval) {
          shoot.shootTimer -= interval;

          // Origem da flecha: posição da mão/arco do arqueiro
          float fwdX = -std::sin(unit.rotationY);
          float fwdZ =  std::cos(unit.rotationY);
          ArrowSpawn spawn;
          spawn.origin = glm::vec3(
              unit.position[0] + fwdX * 0.4f,
              unit.position[1] + 1.2f,
              unit.position[2] + fwdZ * 0.4f);

          // Aponta pro centro do inimigo (altura do peito)
          glm::vec3 toTarget(
              enemyTicks[tidx].position[0] - spawn.origin.x,
              enemyTicks[tidx].position[1] + 0.9f - spawn.origin.y,
              enemyTicks[tidx].position[2] - spawn.origin.z);
          spawn.direction = glm::normalize(toTarget);
          spawn.damage    = dmg;
          spawn.maxDist   = range * 1.5f;
          spawn.targetIdx = tidx;

          result.arrowSpawns.push_back(spawn);
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

      int tidx = findBestTargetInRange(enemies, enemyTicks,
                                       unit.position[0], unit.position[2], range);
      bool enemyInRange = (tidx >= 0);
      float dx = 0.f, dz = 0.f;
      if (enemyInRange) {
        dx = enemyTicks[tidx].position[0] - unit.position[0];
        dz = enemyTicks[tidx].position[2] - unit.position[2];
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
        unit.setAnimation("fire", false);
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
      }

      // Idle com alvo no range: mantém a rotação acompanhando o inimigo.
      if (!shoot.aiming && !shoot.reloading && enemyInRange && tidx >= 0)
        unit.rotationY = -std::atan2(dx, dz);
    }

    // ---- Canhão: idle → fire → cower → idle ----
    if (unit.type == defender_types::kCannon) {
      const float range    = unit.range;
      const int   dmg      = unit.damage;
      const float fireDur  = game_constants::kCannonFireDuration;
      const float cowerDur = game_constants::kCannonCowerDuration;

      int tidx = findBestTargetInRange(enemies, enemyTicks,
                                       unit.position[0], unit.position[2], range);
      bool enemyInRange = (tidx >= 0);
      float dx = 0.f, dz = 0.f;
      if (enemyInRange) {
        dx = enemyTicks[tidx].position[0] - unit.position[0];
        dz = enemyTicks[tidx].position[2] - unit.position[2];
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
            // Estampido/dano/fumaça só com alvo no alcance; senão termina mudo.
            if (tidx >= 0) {
              unit.rotationY = -std::atan2(dx, dz);
              // Dano em área no alvo e em todos no raio de splash.
              combat::applyAreaDamage(enemies, enemyTicks,
                                      enemyTicks[tidx].position[0],
                                      enemyTicks[tidx].position[2],
                                      game_constants::kCannonSplashRadius, dmg);
              ++result.cannonFired;
              using game_constants::kCannonBarrelOffset;
              float fwdX = -std::sin(unit.rotationY);
              float fwdZ =  std::cos(unit.rotationY);
              const float muzzleDist = kCannonBarrelOffset + 1.5f;
              result.cannonShotPositions.push_back(glm::vec3(
                  unit.position[0] + fwdX * muzzleDist,
                  unit.position[1] + 0.35f,
                  unit.position[2] + fwdZ * muzzleDist));
              result.cannonImpactPositions.push_back(glm::vec3(
                  enemyTicks[tidx].position[0],
                  enemyTicks[tidx].position[1] + 0.2f,
                  enemyTicks[tidx].position[2]));
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
            unit.setAnimation("cower", false);
          }
        }
      } else {
        // Idle: acumula a recarga até unit.fireRate e só dispara com alvo E
        // recarga pronta — assim o upgrade de "Recarga" acelera a cadência.
        shoot.shootTimer += deltaTime;
        if (shoot.shootTimer > unit.fireRate) shoot.shootTimer = unit.fireRate;
        if (enemyInRange && shoot.shootTimer >= unit.fireRate) {
          unit.rotationY = -std::atan2(dx, dz);
          unit.setAnimation("fire", false);
          shoot.aiming     = true;
          shoot.reloading  = false;
          shoot.reversing  = false;
          shoot.shootTimer = 0.0f;
        }
      }
    }

    // ---- Comandante de Cavaleiros ----
    if (unit.type == defender_types::kKnight) {
      const float interval = game_constants::kKnightCommandInterval;
      const float animDur  = game_constants::kKnightCommandAnimDuration;

      if (shoot.aiming) {
        // Reproduzindo animação de comando (conclui mesmo sem inimigos)
        shoot.shootTimer += deltaTime;
        if (shoot.shootTimer >= animDur) {
          shoot.aiming     = false;
          shoot.shootTimer = 0.0f;
          unit.setIdleAnimations({"knightIdle"});
        }
      } else if (anyEnemyAlive) {
        shoot.shootTimer += deltaTime;
        if (shoot.shootTimer >= interval) {
          shoot.shootTimer = 0.0f;
          shoot.aiming     = true;
          unit.setAnimation("command", false);
          result.knightSummoned += (unit.damage > 0) ? unit.damage : 1;
          ++result.chargePlayed;
        }
      }
    }

    unit.update(deltaTime);
  }
  return result;
}
