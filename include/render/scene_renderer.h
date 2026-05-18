#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <array>
#include <vector>

#include "engine/animated_model.h"
#include "engine/camera.h"
#include "engine/catmull_rom.h"
#include "engine/game_object.h"
#include "engine/lighting.h"
#include "engine/mesh.h"
#include "engine/primitive_meshes.h"
#include "game/enemy_system.h"
#include "game/scene.h"
#include "math/matrix.h"
#include "math/vector.h"
#include "render/shader_uniforms.h"

// =============================================================================
// SCENE RENDERER
// =============================================================================
// Cada função abaixo desenha um "passo" do frame. Recebe handles/uniforms
// cacheados pra evitar overhead de glGet por draw.

// Bundle de texturas usadas pelo chão de grama (5 PBR maps + noise).
struct GrassTextures {
  unsigned int color;
  unsigned int normal;
  unsigned int ao;
  unsigned int roughness;
  unsigned int displacement;
};

// Bundle de texturas usadas pelo caminho de terra (5 PBR maps + noise).
struct DirtTextures {
  unsigned int color;
  unsigned int normal;
  unsigned int ao;
  unsigned int roughness;
  unsigned int displacement;
};

// Bundle de texturas das lanternas (6 PBR maps).
struct LanternTextures {
  unsigned int color;
  unsigned int normal;
  unsigned int roughness;
  unsigned int metallic;
  unsigned int ao;
  unsigned int opacity;
};

void renderEnemy(GLuint shaderAnim,
                 const EnemyInstance &enemy,
                 GameObject &enemyModel,
                 const Vector<3> &enemyPos,
                 float enemyAngle,
                 unsigned int enemyTexture,
                 unsigned int defaultNormal);

void renderDefenders(GLuint shaderAnim,
                     std::vector<GameObject> &defenders,
                     unsigned int archerTex,
                     unsigned int archerNormal);

void renderDefenderWeapons(GLuint objShader,
                           const ObjUniforms &u,
                           std::vector<GameObject> &defenders,
                           Mesh &bowMesh,
                           unsigned int bowTexture);

void renderCastle(GLuint objShader,
                  const ObjUniforms &u,
                  Mesh &castleMesh,
                  unsigned int castleTex,
                  unsigned int castleNormal,
                  const std::vector<Point> &curvePoints);

void renderGround(const GpuMesh &grassMesh,
                  const GrassTextures &textures,
                  unsigned int noiseTexture);

void renderTrees(GLuint objShader,
                 const ObjUniforms &u,
                 const std::vector<TreeInstance> &trees,
                 Mesh *treeLogMesh,
                 unsigned int treeLogTexture,
                 Mesh *treeLeavesMesh,
                 unsigned int treeLeavesTexture);

void renderLanterns(const LanternUniforms &u,
                    const std::vector<LanternInstance> &lanterns,
                    Mesh &lanternMesh,
                    const LanternTextures &textures);

void renderPath(const PathUniforms &u,
                Mesh &pathMesh,
                const DirtTextures &textures,
                unsigned int noiseTexture,
                const Matrix<4, 4> &identity);

void renderSky(GLuint skyShader,
               GLint skyViewLoc,
               GLint skyProjLoc,
               GLint skyTexLoc,
               const GpuMesh &skyMesh,
               unsigned int skyTexture,
               const Camera &cam,
               const std::array<float, 16> &glProj);

void renderCurveOverlay(GLuint lineShader,
                        const LineUniforms &u,
                        const GpuMesh &curveMesh,
                        const std::array<float, 16> &glView,
                        const std::array<float, 16> &glProj);

// Atualiza as uniforms compartilhadas (matrizes + luz + viewPos) num shader.
void uploadCommonUniforms(GLuint program,
                          const DirectionalLight &moonLight,
                          const std::vector<PointLight> &lanternLights,
                          const glm::vec3 &viewPos,
                          const std::array<float, 16> &glView,
                          const std::array<float, 16> &glProj);
