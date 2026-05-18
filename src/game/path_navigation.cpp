#include "game/path_navigation.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

PathCache buildPathCache(const std::vector<Point> &points) {
  PathCache cache;
  if (points.empty()) {
    std::cerr << "Erro: nenhum ponto enviado para buildPathCache." << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // Primeiro valor sempre é 0
  cache.accumulatedDistances.push_back(0.0f);
  float total = 0.0f;

  // Percorre os segmentos calculando o comprimento e acumulando distância
  for (size_t i = 0; i < points.size() - 1; ++i) {
    float dx = points[i + 1].x - points[i].x;
    float dy = points[i + 1].y - points[i].y;
    total += std::sqrt(dx * dx + dy * dy);
    cache.accumulatedDistances.push_back(total);
  }

  cache.totalDistance = total;
  return cache;
}

namespace {

// Distância 2D do ponto p ao segmento [a, b]. Privada à TU; só distanceToPath
// precisa dela.
float distancePointSegment2D(const Point &p, const Point &a, const Point &b) {
  const float vx = b.x - a.x;
  const float vy = b.y - a.y;
  const float wx = p.x - a.x;
  const float wy = p.y - a.y;

  const float c1 = vx * wx + vy * wy;
  if (c1 <= 0.0f) {
    return std::sqrt(wx * wx + wy * wy);
  }

  const float c2 = vx * vx + vy * vy;
  if (c2 <= c1) {
    const float dx = p.x - b.x;
    const float dy = p.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
  }

  const float t = c1 / c2;
  const float projx = a.x + t * vx;
  const float projy = a.y + t * vy;
  const float dx = p.x - projx;
  const float dy = p.y - projy;
  return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

float distanceToPath(const std::vector<Point> &curvePoints, float x, float z) {
  if (curvePoints.size() < 2) {
    return std::numeric_limits<float>::max();
  }

  const Point p{x, z};
  float minDist = std::numeric_limits<float>::max();
  for (size_t i = 0; i + 1 < curvePoints.size(); ++i) {
    const float d = distancePointSegment2D(p, curvePoints[i], curvePoints[i + 1]);
    if (d < minDist) {
      minDist = d;
    }
  }

  return minDist;
}

Vector<3> getPositionAtDistance(const std::vector<Point> &points,
                                const PathCache &cache,
                                float distance,
                                float &outAngle,
                                bool &hasReachedEnd) {
  if (points.size() < 2 || cache.totalDistance <= 0.0f) {
    std::cerr << "Erro: path inválido." << std::endl;
    std::exit(EXIT_FAILURE);
  }

  if (distance <= 0.0f) {
    distance = 0.0f;
  } else if (distance >= cache.totalDistance) {
    distance = cache.totalDistance;
    hasReachedEnd = true;
  }

  // Encontra o segmento em que o personagem está
  auto it = std::upper_bound(
      cache.accumulatedDistances.begin(), cache.accumulatedDistances.end(), distance);
  size_t i = std::distance(cache.accumulatedDistances.begin(), it) - 1;

  // Proteção para não estourar o índice quando distance == totalDistance
  if (i >= points.size() - 1) {
    i = points.size() - 2;
  }

  Point p1 = points[i];
  Point p2 = points[i + 1];

  // Interpolação linear dentro do segmento
  float distToP1 = distance - cache.accumulatedDistances[i];
  float segLen = cache.accumulatedDistances[i + 1] - cache.accumulatedDistances[i];
  float t = distToP1 / segLen;

  // Yaw que aponta para o próximo ponto
  float dx = p2.x - p1.x;
  float dy = p2.y - p1.y;
  outAngle = std::atan2(-dx, dy);

  return {p1.x + dx * t, 0.1f, p1.y + dy * t};
}
