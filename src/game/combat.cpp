#include "game/combat.h"

#include "game/collisions.h"
#include "game/game_constants.h"

namespace combat {

int applyAreaDamage(std::vector<EnemyInstance> &enemies,
                    const std::vector<EnemyTickResult> &ticks,
                    float cx, float cz, float radius, int damage) {
  int hits = 0;
  for (size_t i = 0; i < enemies.size(); ++i) {
    if (!enemies[i].alive) continue;
    if (!collisions::inRange(cx, cz, ticks[i].position[0], ticks[i].position[2], radius))
      continue;
    enemies[i].hp -= damage;
    enemies[i].hitFlashTime = game_constants::kEnemyHitFlashDuration;
    if (enemies[i].hp < 0) enemies[i].hp = 0;
    ++hits;
  }
  return hits;
}

}  // namespace combat
