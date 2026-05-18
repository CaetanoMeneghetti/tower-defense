#pragma once

#include <vector>

#include "engine/catmull_rom.h"
#include "math/vector.h"

// =============================================================================
// NAVEGAÇÃO POR DISTÂNCIA AO LONGO DO PATH
// =============================================================================
// Após gerar a curva Catmull-Rom, pré-calculamos distâncias acumuladas para
// poder responder "onde estou se andei X metros desde o início?" em O(log n).

struct PathCache {
  std::vector<float> accumulatedDistances;
  float totalDistance = 0.0f;
};

PathCache buildPathCache(const std::vector<Point> &points);

// Distância mínima do ponto (x, z) à polyline completa do path.
float distanceToPath(const std::vector<Point> &curvePoints, float x, float z);

// Posição 3D ao longo do path para um deslocamento `distance` em metros.
// `outAngle` recebe o yaw para alinhar entidades à direção do caminho.
// `hasReachedEnd` vira true quando `distance >= totalDistance`.
Vector<3> getPositionAtDistance(const std::vector<Point> &points,
                                const PathCache &cache,
                                float distance,
                                float &outAngle,
                                bool &hasReachedEnd);
