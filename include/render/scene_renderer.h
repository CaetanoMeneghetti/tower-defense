#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <vector>

#include "game/arrow_system.h"

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

void renderKnight(GLuint shaderAnim,
                  GameObject &knightModel,
                  const Vector<3> &knightPos,
                  float knightAngle,
                  unsigned int knightTexture,
                  unsigned int knightNormal);

struct KnightWeaponTweaks {
  float sword_tx    =  31.250f;
  float sword_ty    =  -6.250f;
  float sword_tz    = -37.500f;
  float sword_rotX  =  -0.196f;
  float sword_rotY  =  -0.589f;
  float sword_rotZ  =   1.374f;
  float sword_scale = 128.031f;

  float shield_tx    =   3.750f;
  float shield_ty    = -43.750f;
  float shield_tz    =  -8.750f;
  float shield_rotX  =   0.000f;
  float shield_rotY  =   2.454f;
  float shield_rotZ  =   0.000f;
  float shield_scale = 180.000f;
};

void renderKnightWeapons(GLuint objShader,
                         const ObjUniforms &u,
                         GameObject &knightModel,
                         Mesh &swordMesh,
                         unsigned int swordTexture,
                         Mesh &shieldMesh,
                         unsigned int shieldTexture,
                         const KnightWeaponTweaks &tweaks);

void renderDefenders(GLuint shaderAnim,
                     std::vector<GameObject> &defenders,
                     unsigned int archerTex,
                     unsigned int archerNormal);

void renderDefenderWeapons(GLuint objShader,
                           const ObjUniforms &u,
                           std::vector<GameObject> &defenders,
                           Mesh &bowMesh,
                           unsigned int bowTexture,
                           Mesh &torchMesh,
                           unsigned int torchTexture);

void renderCannonBarrels(GLuint objShader,
                         const ObjUniforms &u,
                         const std::vector<GameObject> &defenders,
                         Mesh &cannonBarrelMesh,
                         unsigned int cannonBarrelTex);

struct CastleGroup {
  std::unique_ptr<Mesh> mesh;
  GLuint colorTex  = 0;
  GLuint normalTex = 0;
};

void renderCastle(GLuint objShader,
                  const ObjUniforms &u,
                  const std::vector<CastleGroup> &groups,
                  const std::vector<Point> &curvePoints);

void renderArrows(GLuint objShader,
                  const ObjUniforms &u,
                  const std::vector<ArrowProjectile> &arrows,
                  Mesh &arrowMesh,
                  GLuint arrowTex);

void renderGround(const GpuMesh &grassMesh,
                  const GrassTextures &textures,
                  unsigned int noiseTexture);

// Desenha todas as árvores via instancing: 1 draw call para os troncos e 1 para
// as folhagens, em vez de 2 por árvore. As VAOs dos meshes devem ter os atributos
// de matriz por-instância (locations 3..6) já anexados (ver setupTreeInstancing).
void renderTrees(GLuint treeShader,
                 const std::vector<TreeInstance> &trees,
                 Mesh *treeLogMesh,
                 unsigned int treeLogTexture,
                 Mesh *treeLeavesMesh,
                 unsigned int treeLeavesTexture);

// Cria o VBO de matrizes por-instância (uma por árvore) e anexa os atributos
// instanciados (locations 3..6, divisor 1) às VAOs dos meshes informados.
// Retorna o id do VBO (libere com glDeleteBuffers no cleanup).
GLuint setupTreeInstancing(const std::vector<TreeInstance> &trees,
                           Mesh *treeLogMesh,
                           Mesh *treeLeavesMesh);

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
