#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

#include "engine/animated_model.h"
#include "engine/catmull_rom.h"

// =============================================================================
// GRAMA COM BALANÇO PROCEDURAL — completamente isolado do resto do render
// =============================================================================

struct GrassField {
  GLuint shader        = 0;
  GLuint texture       = 0;
  GLuint instanceVBO   = 0;
  GLuint normalVBO     = 0;
  int    instanceCount = 0;
};

// Gera instâncias de grama evitando o caminho real (curvePoints).
// pathClearance = distância mínima do centro do path pra uma muda nascer.
GrassField buildGrassField(GLuint shader, GLuint texture,
                           const std::vector<Point> &curvePoints,
                           float pathClearance,
                           float areaHalfX, float areaHalfZ,
                           float spacing, float baseScale);

// Renderiza todas as instâncias com o grass_sway shader.
void renderGrassField(const GrassField &field,
                      AnimatedModel    &model,
                      float             time,
                      const glm::vec3  &lightDir,
                      const glm::vec3  &lightAmbient,
                      const glm::vec3  &lightDiffuse,
                      const glm::vec3  &fogColor,
                      float             fogStart,
                      float             fogEnd,
                      const glm::vec3  &viewPos,
                      const glm::mat4  &view,
                      const glm::mat4  &proj);
