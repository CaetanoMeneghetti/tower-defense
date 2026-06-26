#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

#include "engine/catmull_rom.h"
#include "engine/lighting.h"

// =============================================================================
// GRAMA COM BALANÇO PROCEDURAL — mesh .obj + GPU instancing
// =============================================================================

struct GrassField {
  GLuint shader        = 0;
  GLuint texture       = 0;
  GLuint meshVAO       = 0;  // VAO com verts + atribs de instância ligados
  GLuint meshVBO       = 0;  // dados de vértice (Vertex: pos+uv+normal, 32 bytes)
  GLuint instanceVBO   = 0;  // mat4 por instância (locations 3-6)
  GLuint normalVBO     = 0;  // mat3 normal pré-computada (locations 7-9)
  int    vertexCount   = 0;
  int    instanceCount = 0;
};

// Carrega grass.glb, gera instâncias evitando o caminho e monta os VBOs de GPU.
GrassField buildGrassField(GLuint shader, GLuint texture,
                           const std::vector<Point> &curvePoints,
                           float pathClearance,
                           float areaHalfX, float areaHalfZ,
                           float spacing, float baseScale);

// Renderiza todas as instâncias com o grass_sway shader.
void renderGrassField(const GrassField              &field,
                      float                          time,
                      const glm::vec3               &lightDir,
                      const glm::vec3               &lightAmbient,
                      const glm::vec3               &lightDiffuse,
                      const glm::vec3               &fogColor,
                      float                          fogStart,
                      float                          fogEnd,
                      const glm::vec3               &viewPos,
                      const glm::mat4               &view,
                      const glm::mat4               &proj,
                      const std::vector<PointLight> &pointLights);
