#include "game/scene.h"

#include <cmath>
#include <ctime>
#include <random>

#include "engine/lighting.h"
#include "game/game_constants.h"
#include "math/constants.h"

std::vector<TreeInstance> placeTrees(const std::vector<Point> &curvePoints) {
  using namespace game_constants;

  std::vector<TreeInstance> trees;
  trees.reserve(kTreeCount);

  std::mt19937 rng(static_cast<unsigned int>(time(NULL)));
  // Gera candidatos só dentro do raio de falloff (além dele nada é aceito), o que
  // já concentra as tentativas na região visível.
  std::uniform_real_distribution<float> posDist(-kTreeFalloffRadius, kTreeFalloffRadius);
  std::uniform_real_distribution<float> rotDist(0.0f, math_constants::kTwoPi);
  std::uniform_real_distribution<float> scaleDist(0.8f, 1.4f);
  std::uniform_real_distribution<float> keepDist(0.0f, 1.0f);

  const float minSpacingSq = kTreeMinSpacing * kTreeMinSpacing;
  const float pathAvoidDist = kPathHalfWidth + kTreePathBuffer;
  const float falloffSpan = kTreeFalloffRadius - kTreeFullDensityRadius;
  int attempts = 0;
  while (static_cast<int>(trees.size()) < kTreeCount && attempts < kMaxTreeAttempts) {
    ++attempts;

    const float x = posDist(rng);
    const float z = posDist(rng);

    // Redução gradual de densidade conforme se afasta do centro do mapa (0,0).
    // keepProb: 1 até o raio cheio, decai linearmente até 0 no raio de falloff.
    const float radius = std::sqrt(x * x + z * z);
    const float keepProb = (radius <= kTreeFullDensityRadius)
                               ? 1.0f
                               : 1.0f - (radius - kTreeFullDensityRadius) / falloffSpan;
    if (keepProb <= 0.0f || keepDist(rng) > keepProb) {
      continue;
    }

    if (distanceToPath(curvePoints, x, z) < pathAvoidDist) {
      continue;
    }

    bool tooClose = false;
    for (const auto &t : trees) {
      const float dx = x - t.position[0];
      const float dz = z - t.position[2];
      if ((dx * dx + dz * dz) < minSpacingSq) {
        tooClose = true;
        break;
      }
    }
    if (tooClose) {
      continue;
    }

    TreeInstance tree;
    tree.position = Vector<3>{x, 0.0f, z};
    tree.rotationY = rotDist(rng);
    tree.scale = kTreeBaseScale * scaleDist(rng);
    trees.push_back(tree);
  }

  return trees;
}

std::vector<LanternInstance> placeLanterns(const std::vector<Point> &curvePoints,
                                           const PathCache &curveCache,
                                           std::vector<PointLight> &outLanternLights) {
  using namespace game_constants;

  std::vector<LanternInstance> lanterns;
  outLanternLights.clear();

  if (curveCache.totalDistance <= 0.0f) {
    return lanterns;
  }

  // Conta derivada do comprimento do path: uma lanterna a cada kLanternSpacing
  // unidades de arco por lado, cappada para não consumir todos os slots de luz.
  const int maxPerSide = (kMaxPointLights - 4) / 2;
  const int perSide = std::min(maxPerSide,
      std::max(1, static_cast<int>(curveCache.totalDistance / kLanternSpacing)));
  const int totalCount = perSide * 2;
  const float spacing = curveCache.totalDistance / static_cast<float>(perSide);

  lanterns.reserve(totalCount);
  outLanternLights.reserve(totalCount);
  for (int i = 0; i < totalCount; ++i) {
    const int sideIdx = i / 2;
    const bool isLeft = (i % 2 == 0);
    const float phase = isLeft ? 0.25f : 0.75f;
    const float arcLen = (static_cast<float>(sideIdx) + phase) * spacing;

    float pathAngle = 0.0f;
    bool dummyReachedEnd = false;
    Vector<3> pathPos = getPositionAtDistance(
        curvePoints, curveCache, arcLen, pathAngle, dummyReachedEnd);

    // getPositionAtDistance retorna outAngle = atan2(-dx, dy). Recupera a
    // direção 2D normalizada do path neste ponto.
    const float ndx = -std::sin(pathAngle);
    const float ndy = std::cos(pathAngle);

    // Perpendicular esquerda em XZ (rot 90° anti-horário em torno de Y).
    const float perpX = -ndy;
    const float perpZ = ndx;
    const float side = isLeft ? 1.0f : -1.0f;

    LanternInstance lantern;
    lantern.position = Vector<3>{pathPos[0] + perpX * kLanternPathOffset * side,
                                 0.0f,
                                 pathPos[2] + perpZ * kLanternPathOffset * side};

    // Lanterna olha para o centro do path: vetor da lanterna até o path.
    const float towardPathX = -perpX * side;
    const float towardPathZ = -perpZ * side;
    lantern.rotationY = std::atan2(towardPathX, towardPathZ);

    lanterns.push_back(lantern);

    PointLight pl;
    pl.position = glm::vec3(lantern.position[0], kLanternLightHeight, lantern.position[2]);
    pl.color = kLanternLightColor;
    outLanternLights.push_back(pl);
  }

  return lanterns;
}
