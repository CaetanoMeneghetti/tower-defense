#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "engine/catmull_rom.h"
#include "engine/lighting.h"
#include "game/path_navigation.h"
#include "math/vector.h"

// =============================================================================
// SCENE PLACEMENT — ÁRVORES + LANTERNAS
// =============================================================================
// Geração procedural única (na inicialização) do ambiente em volta do path.

struct TreeInstance {
  Vector<3> position;
  float rotationY = 0.0f;
  float scale = 1.0f;
};

struct LanternInstance {
  Vector<3> position;
  float rotationY = 0.0f;
};

// Espalha N árvores aleatórias evitando o path e mantendo espaçamento mínimo.
std::vector<TreeInstance> placeTrees(const std::vector<Point> &curvePoints);

// Distribui lanternas em zigue-zague ao longo do path. Preenche
// outLanternLights com a luz pontual associada a cada lanterna.
std::vector<LanternInstance> placeLanterns(const std::vector<Point> &curvePoints,
                                           const PathCache &curveCache,
                                           std::vector<PointLight> &outLanternLights);
