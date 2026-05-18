#include "game/defender_system.h"

#include <cmath>

#include "game/game_constants.h"

void updateDefenders(std::vector<GameObject> &defenders,
                     std::vector<DefenderShoot> &defenderShoots,
                     EnemyInstance &enemy,
                     const Vector<3> &enemyPos,
                     float deltaTime) {
  // Mantem o vetor paralelo do mesmo tamanho dos defenders.
  if (defenderShoots.size() < defenders.size()) {
    defenderShoots.resize(defenders.size(), DefenderShoot{0.0f, false});
  }

  for (size_t i = 0; i < defenders.size(); ++i) {
    auto &unit = defenders[i];
    auto &shoot = defenderShoots[i];

    // Apenas arqueiros atiram por enquanto — arcabuz nao tem animacao de aim
    // carregada.
    if (unit.type == defender_types::kArcher) {
      bool canShoot = false;
      float dxToEnemy = 0.0f;
      float dzToEnemy = 0.0f;
      if (enemy.alive) {
        dxToEnemy = enemyPos[0] - unit.position[0];
        dzToEnemy = enemyPos[2] - unit.position[2];
        const float distSq = dxToEnemy * dxToEnemy + dzToEnemy * dzToEnemy;
        canShoot = (distSq <= game_constants::kArcherRange * game_constants::kArcherRange);
      }

      if (canShoot) {
        // Vira o arqueiro para o inimigo (mesma convencao usada pelas lanternas
        // e pelo enemyRunner que segue o path).
        unit.rotationY = -std::atan2(dxToEnemy, dzToEnemy);

        if (!shoot.aiming) {
          unit.setAnimation("aim");
          shoot.aiming = true;
          shoot.shootTimer = 0.0f;
        }
        shoot.shootTimer += deltaTime;
        if (shoot.shootTimer >= game_constants::kArcherShootInterval) {
          shoot.shootTimer -= game_constants::kArcherShootInterval;
          enemy.hp -= game_constants::kArcherArrowDamage;
          enemy.hitFlashTime = game_constants::kEnemyHitFlashDuration;
          if (enemy.hp <= 0) {
            enemy.hp = 0;
            enemy.alive = false;
            enemy.respawnTimer = game_constants::kEnemyRespawnDelay;
          }
        }
      } else if (shoot.aiming) {
        shoot.aiming = false;
        shoot.shootTimer = 0.0f;
        unit.setIdleAnimations({"idle1"});
      }
    }

    unit.update(deltaTime);
  }
}
