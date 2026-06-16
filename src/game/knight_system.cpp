#include "game/knight_system.h"

#include <cmath>

#include "game/collisions.h"
#include "game/game_constants.h"

KnightInstance makeKnight(const PathCache &curveCache) {
  KnightInstance k;
  k.maxHp          = game_constants::kKnightMaxHp;
  k.hp             = k.maxHp;
  k.speed          = game_constants::kKnightSpeed;
  k.contactDamage  = game_constants::kKnightSlashDamage;
  k.slashInterval  = game_constants::kKnightSlashInterval;
  k.slashTimer     = 0.0f;
  k.pathDistance   = curveCache.totalDistance;
  k.collisionRadius = game_constants::kKnightCollisionRadius;
  k.alive          = true;
  k.slashing       = false;
  return k;
}

KnightTickResult updateKnight(KnightInstance &knight,
                              GameObject &knightModel,
                              std::vector<EnemyInstance> &enemies,
                              const std::vector<EnemyTickResult> &enemyTicks,
                              const std::vector<Point> &curvePoints,
                              const PathCache &curveCache,
                              float deltaTime) {
  KnightTickResult result;
  result.angle        = 0.0f;
  result.hitThisFrame = false;

  if (knight.dying) {
    knight.deathTimer += deltaTime;
    knightModel.update(deltaTime);
    bool dummy = false;
    result.position = getPositionAtDistance(
        curvePoints, curveCache, knight.pathDistance, result.angle, dummy);
    result.angle += 3.14159265f;
    if (knight.deathTimer >= 2.0f) {
      knight.dying = false;
    }
    return result;
  }

  if (!knight.alive) {
    result.position = Vector<3>{0.0f, -1000.0f, 0.0f};
    return result;
  }

  bool dummyReached = false;
  float pathAngle   = 0.0f;
  result.position   = getPositionAtDistance(
      curvePoints, curveCache, knight.pathDistance, pathAngle, dummyReached);
  result.angle = pathAngle + 3.14159265f;

  float closestDistSq = 0.0f;
  int targetIdx = collisions::closestAliveEnemy(
      enemies, enemyTicks,
      result.position[0], result.position[2],
      closestDistSq);
  const float r = knight.collisionRadius;
  bool colliding = (targetIdx >= 0) && (closestDistSq <= r * r);

  if (colliding) {
    if (!knight.slashing) {
      knightModel.setAnimation("knightSlash");
      knight.slashing  = true;
      knight.slashTimer = 0.0f;
    }
    knight.slashTimer += deltaTime;
    if (knight.slashTimer >= knight.slashInterval) {
      knight.slashTimer -= knight.slashInterval;
      EnemyInstance &target = enemies[targetIdx];
      target.hp -= knight.contactDamage;
      target.hitFlashTime = game_constants::kEnemyHitFlashDuration;
      if (target.hp <= 0) target.hp = 0;
      result.hitThisFrame = true;

      knight.hp -= target.stats.damage;
      if (knight.hp <= 0) {
        knight.hp        = 0;
        knight.alive     = false;
        knight.slashing  = false;
        knight.dying     = true;
        knight.deathTimer = 0.0f;
        knightModel.setAnimation("knightDeath", false);
      }
    }
  } else {
    if (knight.slashing) {
      knight.slashing  = false;
      knight.slashTimer = 0.0f;
      knightModel.setIdleAnimations({"knightWalk"});
      knightModel.setAnimation("knightWalk");
    }
    knight.pathDistance -= knight.speed * deltaTime;
    if (knight.pathDistance < 0.0f) knight.pathDistance = 0.0f;
  }

  knightModel.update(deltaTime);
  return result;
}
