#pragma once

#include <glad/glad.h>
#include <vector>

#include "engine/catmull_rom.h"

// =============================================================================
// MESHES PRIMITIVAS GERADAS NO GPU
// =============================================================================
// Versões simples de mesh (sem material/textura), usadas para o skybox,
// o chão de grama e a curva de debug do path.

struct GpuMesh {
  unsigned int vao = 0;
  unsigned int vbo = 0;
  GLsizei vertexCount = 0;
};

// Cubo unitário invertido usado como skybox (apenas posições XYZ).
GpuMesh createSkyboxMesh();

// Plano [-200, 200] no XZ com UVs (5 floats por vértice).
GpuMesh createGrassMesh();

// Line strip da curva-guia (apenas posições XYZ; Y = 0.1 para evitar
// z-fighting com o chão).
GpuMesh createCurveMesh(const std::vector<Point> &curvePoints);

// Libera os recursos GL associados.
void deleteGpuMesh(GpuMesh &mesh);
