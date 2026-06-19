#include "game/arrow_system.h"

#include <algorithm>
#include <cmath>

#include "game/game_constants.h"

static constexpr float kHitRadius = 0.6f;  // distância para considerar acerto

void updateArrows(std::vector<ArrowProjectile>      &arrows,
                  std::vector<EnemyInstance>         &enemies,
                  const std::vector<EnemyTickResult> &ticks,
                  float                               deltaTime) {
  for (auto &arrow : arrows) {
    if (arrow.traveled >= arrow.maxDist) continue;

    // Se o alvo ainda está vivo, atualiza direção para seguir o inimigo
    int t = arrow.targetIdx;
    if (t >= 0 && t < static_cast<int>(enemies.size()) && enemies[t].alive) {
      glm::vec3 target(ticks[t].position[0],
                       ticks[t].position[1] + 0.9f,
                       ticks[t].position[2]);
      glm::vec3 toTarget = target - arrow.position;
      float dist = glm::length(toTarget);

      if (dist < kHitRadius) {
        // Acertou
        enemies[t].hp          -= arrow.damage;
        enemies[t].hitFlashTime = game_constants::kEnemyHitFlashDuration;
        if (enemies[t].hp <= 0) enemies[t].hp = 0;
        arrow.traveled = arrow.maxDist;
        continue;
      }

      arrow.velocity = (toTarget / dist) * kArrowSpeed;
    }

    arrow.position += arrow.velocity * deltaTime;
    arrow.traveled += kArrowSpeed * deltaTime;
  }

  arrows.erase(
      std::remove_if(arrows.begin(), arrows.end(),
                     [](const ArrowProjectile &a) { return a.traveled >= a.maxDist; }),
      arrows.end());
}
