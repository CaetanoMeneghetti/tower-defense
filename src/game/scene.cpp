#include "game/scene.h"

#include <cmath>
#include <ctime>
#include <random>

#include "game/game_constants.h"
#include "math/constants.h"

std::vector<TreeInstance> placeTrees(const std::vector<Point> &curvePoints) {
  using namespace game_constants;

  std::vector<TreeInstance> trees;
  trees.reserve(kTreeCount);

  std::mt19937 rng(static_cast<unsigned int>(time(NULL)));
  std::uniform_real_distribution<float> posDist(
      -kMapHalfSize + kTreeBorderMargin, kMapHalfSize - kTreeBorderMargin);
  std::uniform_real_distribution<float> rotDist(0.0f, math_constants::kTwoPi);
  std::uniform_real_distribution<float> scaleDist(0.8f, 1.4f);

  const float minSpacingSq = kTreeMinSpacing * kTreeMinSpacing;
  const float pathAvoidDist = kPathHalfWidth + kTreePathBuffer;
  int attempts = 0;
  while (static_cast<int>(trees.size()) < kTreeCount && attempts < kMaxTreeAttempts) {
    ++attempts;

    const float x = posDist(rng);
    const float z = posDist(rng);

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
  lanterns.reserve(kLanternCount);
  outLanternLights.clear();
  outLanternLights.reserve(kLanternCount);

  if (curveCache.totalDistance <= 0.0f) {
    return lanterns;
  }

  // Duas sequências interleaved: kLanternCount/2 lanternas em cada lado, com
  // espaçamento próprio uniforme. Lado direito sai 0.5 spacing à frente do
  // esquerdo para o efeito zigue-zague — evita aglomeração nas curvas.
  const int perSide = kLanternCount / 2;
  const float spacing = curveCache.totalDistance / static_cast<float>(perSide);
  for (int i = 0; i < kLanternCount; ++i) {
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
