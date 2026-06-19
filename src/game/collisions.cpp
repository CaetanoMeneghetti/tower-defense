#include "game/collisions.h"

#include <cfloat>
#include <cmath>

#include "game/path_navigation.h"

namespace collisions {

bool inRange(float ax, float az, float bx, float bz, float range) {
  const float dx = bx - ax, dz = bz - az;
  return (dx * dx + dz * dz) <= (range * range);
}

bool aabbOverlap(float ax, float az, float aHalf,
                 float bx, float bz, float bHalf) {
  return std::fabs(ax - bx) <= (aHalf + bHalf) &&
         std::fabs(az - bz) <= (aHalf + bHalf);
}

int closestAliveEnemy(const std::vector<EnemyInstance> &enemies,
                      const std::vector<EnemyTickResult> &ticks,
                      float ox, float oz,
                      float &outDistSq) {
  int   best  = -1;
  float minSq = FLT_MAX;
  for (int i = 0; i < (int)enemies.size(); ++i) {
    if (!enemies[i].alive) continue;
    const float dx = ticks[i].position[0] - ox;
    const float dz = ticks[i].position[2] - oz;
    const float sq = dx * dx + dz * dz;
    if (sq < minSq) { minSq = sq; best = i; }
  }
  outDistSq = (best >= 0) ? minSq : FLT_MAX;
  return best;
}

PlacementResult checkPlacement(const std::vector<Point> &curvePoints,
                                float px, float pz,
                                float pathHalfWidth, float pathClearance,
                                float maxDistance) {
  const float dist = distanceToPath(curvePoints, px, pz);
  return { dist < (pathHalfWidth + pathClearance), dist > maxDistance };
}

}  // namespace collisions
