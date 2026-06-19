#include "render/scene_renderer.h"

#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <string>

#include "game/defender_system.h"
#include "game/game_constants.h"
#include "math/constants.h"
#include "math/matrix_ops.h"
#include "math/opengl_utils.h"
#include "math/transforms.h"

namespace {

// Helper: bind sequencial de N texturas a partir do TEXTURE0.
void bindTextureChain(std::initializer_list<unsigned int> textures) {
  int unit = 0;
  for (unsigned int tex : textures) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex);
    ++unit;
  }
}

// Helper para desenhar uma instância única (com translate, rotateY e scale uniforme)
// usando o uniform "model" do shader OBJ padrão.
void uploadModelMatrix(GLint modelLoc, const Vector<3> &pos, float rotY, float uniformScale) {
  Matrix<4, 4> t = translate<4, 4>(pos[0], pos[1], pos[2]);
  Matrix<4, 4> r = rotateY<4, 4>(rotY);
  Matrix<4, 4> s = scale<4, 4>(uniformScale, uniformScale, uniformScale);
  Matrix<4, 4> model = t * r * s;
  auto gl = toOpenGLMatrix(model);
  glUniformMatrix4fv(modelLoc, 1, GL_FALSE, gl.data());
}

void bindColorAndNormal(GLuint shader, unsigned int color, unsigned int normal) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, color);
  glUniform1i(cachedUniformLocation(shader, "tex"), 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, normal);
  glUniform1i(cachedUniformLocation(shader, "normalMap"), 1);
}

}  // namespace

void uploadCommonUniforms(GLuint program,
                          const DirectionalLight &moonLight,
                          const std::vector<PointLight> &lanternLights,
                          const glm::vec3 &viewPos,
                          const std::array<float, 16> &glView,
                          const std::array<float, 16> &glProj) {
  glUseProgram(program);
  applyDirectionalLight(program, moonLight, viewPos);
  applyPointLights(program, lanternLights);
  glUniformMatrix4fv(cachedUniformLocation(program, "view"), 1, GL_FALSE, glView.data());
  glUniformMatrix4fv(cachedUniformLocation(program, "projection"), 1, GL_FALSE, glProj.data());
}

// =============================================================================
// INIMIGO
// =============================================================================

void renderEnemy(GLuint shaderAnim,
                 const EnemyInstance &enemy,
                 GameObject &enemyModel,
                 const Vector<3> &enemyPos,
                 float enemyAngle,
                 unsigned int enemyTexture,
                 unsigned int defaultNormal) {
  

  bindColorAndNormal(shaderAnim, enemyTexture, defaultNormal);

  enemyModel.position = enemyPos;
  enemyModel.rotationY = enemyAngle;

  const float flash = (game_constants::kEnemyHitFlashDuration > 0.0f)
                          ? (enemy.hitFlashTime / game_constants::kEnemyHitFlashDuration)
                          : 0.0f;
  glUniform1f(cachedUniformLocation(shaderAnim, "hitFlash"), flash);
  enemyModel.draw(shaderAnim);

  // Reseta pra nao vazar flash para os defensores no mesmo shader.
  glUniform1f(cachedUniformLocation(shaderAnim, "hitFlash"), 0.0f);
}


// =============================================================================
// KNIGHT
// =============================================================================

void renderKnight(GLuint shaderAnim,
                  GameObject &knightModel,
                  const Vector<3> &knightPos,
                  float knightAngle,
                  unsigned int knightTexture,
                  unsigned int knightNormal) {
  bindColorAndNormal(shaderAnim, knightTexture, knightNormal);

  knightModel.position = knightPos;
  knightModel.rotationY = knightAngle;

  glUniform1f(cachedUniformLocation(shaderAnim, "hitFlash"), 0.0f);
  knightModel.draw(shaderAnim);
}

// Matriz de escala uniforme (coluna-major, formato glm).
static glm::mat4 weaponScaleMat(float s) {
  glm::mat4 m(0.0f);
  m[0][0] = s; m[1][1] = s; m[2][2] = s; m[3][3] = 1.0f;
  return m;
}

// Matriz de translação 
// Ajuste tx/ty/tz em unidades do bone (100 unidades ≈ 1 unidade no mundo).
static glm::mat4 weaponTranslateMat(float tx, float ty, float tz) {
  glm::mat4 m(0.0f);
  m[0][0] = 1.0f; m[1][1] = 1.0f; m[2][2] = 1.0f; m[3][3] = 1.0f;
  m[3][0] = tx;   m[3][1] = ty;   m[3][2] = tz;
  return m;
}

// Rotação em torno do eixo X 
static glm::mat4 weaponRotateXMat(float rad) {
  float c = std::cos(rad), s = std::sin(rad);
  glm::mat4 m(0.0f);
  m[0][0] = 1.0f;
  m[1][1] =  c;  m[1][2] = s;
  m[2][1] = -s;  m[2][2] = c;
  m[3][3] = 1.0f;
  return m;
}

// Rotação em torno do eixo Y 
static glm::mat4 weaponRotateYMat(float rad) {
  float c = std::cos(rad), s = std::sin(rad);
  glm::mat4 m(0.0f);
  m[0][0] =  c;  m[0][2] = -s;
  m[1][1] = 1.0f;
  m[2][0] =  s;  m[2][2] =  c;
  m[3][3] = 1.0f;
  return m;
}

// Rotação em torno do eixo Z 
static glm::mat4 weaponRotateZMat(float rad) {
  float c = std::cos(rad), s = std::sin(rad);
  glm::mat4 m(0.0f);
  m[0][0] =  c;  m[0][1] = s;
  m[1][0] = -s;  m[1][1] = c;
  m[2][2] = 1.0f;
  m[3][3] = 1.0f;
  return m;
}

void renderKnightWeapons(GLuint objShader,
                         const ObjUniforms &u,
                         GameObject &knightModel,
                         Mesh &swordMesh,
                         unsigned int swordTexture,
                         Mesh &shieldMesh,
                         unsigned int shieldTexture,
                         const KnightWeaponTweaks &t) {
  // --- ESPADA (mão direita) ---
  glm::mat4 swordOffset = weaponTranslateMat(t.sword_tx, t.sword_ty, t.sword_tz)
                        * weaponRotateXMat(t.sword_rotX)
                        * weaponRotateYMat(t.sword_rotY)
                        * weaponRotateZMat(t.sword_rotZ)
                        * weaponScaleMat(t.sword_scale);

  glm::mat4 rightHand = knightModel.getBoneWorldTransform("mixamorig:RightHand");
  glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(rightHand * swordOffset));
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, swordTexture);
  glUniform1i(cachedUniformLocation(objShader, "tex"), 0);
  swordMesh.draw();

  // --- ESCUDO (mão esquerda) ---
  glm::mat4 shieldOffset = weaponTranslateMat(t.shield_tx, t.shield_ty, t.shield_tz)
                         * weaponRotateXMat(t.shield_rotX)
                         * weaponRotateYMat(t.shield_rotY)
                         * weaponRotateZMat(t.shield_rotZ)
                         * weaponScaleMat(t.shield_scale);

  glm::mat4 leftHand = knightModel.getBoneWorldTransform("mixamorig:LeftHand");
  glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(leftHand * shieldOffset));
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, shieldTexture);
  glUniform1i(cachedUniformLocation(objShader, "tex"), 0);
  shieldMesh.draw();
}

// =============================================================================
// DEFENSORES (corpo animado)
// =============================================================================

void renderDefenders(GLuint shaderAnim,
                     std::vector<GameObject> &defenders,
                     unsigned int archerTex,
                     unsigned int archerNormal) {
  for (auto &unit : defenders) {
    if (unit.type == defender_types::kArcher) {
      bindColorAndNormal(shaderAnim, archerTex, archerNormal);
    } else if (unit.type == defender_types::kArquebus ||
               unit.type == defender_types::kCannon  ||
               unit.type == defender_types::kKnight) {
      const TroopTier &tier = unit.getCurrentTier();
      bindColorAndNormal(shaderAnim, tier.texture, tier.normalMap);
    }

    if (unit.type == defender_types::kCannon) {
      // Recua o canhoneiro para trás do canh��o antes de desenhar.
      const float behind = game_constants::kCannonerBehindOffset;
      const float bkX =  std::sin(unit.rotationY);   // oposto de forward.x
      const float bkZ = -std::cos(unit.rotationY);   // oposto de forward.z
      float ox = unit.position[0];
      float oz = unit.position[2];
      unit.position[0] += bkX * behind;
      unit.position[2] += bkZ * behind;
      unit.draw(shaderAnim);
      unit.position[0] = ox;
      unit.position[2] = oz;
    } else {
      unit.draw(shaderAnim);
    }
  }
}

// =============================================================================
// ARMAS DOS DEFENSORES (anexadas ao bone da mão)
// =============================================================================

void renderDefenderWeapons(GLuint objShader,
                           const ObjUniforms &u,
                           std::vector<GameObject> &defenders,
                           Mesh &bowMesh,
                           unsigned int bowTexture,
                           Mesh &torchMesh,
                           unsigned int torchTexture) {
  for (auto &unit : defenders) {
    if (unit.type == defender_types::kArcher) {
      glm::mat4 handWorldMatrix = unit.getBoneWorldTransform("mixamorig:LeftHand");

      glm::mat4 offset = glm::mat4(0.0f);
      float s = 5.0f;
      offset[0][2] = s;
      offset[1][1] = s;
      offset[2][0] = -s;
      offset[3][3] = 1.0f;
      offset[3][0] = 0.0f;
      offset[3][1] = -7.0f;
      offset[3][2] = 0.0f;

      glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(handWorldMatrix * offset));
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, bowTexture);
      glUniform1i(cachedUniformLocation(objShader, "tex"), 0);
      bowMesh.draw();

    } else if (unit.type == defender_types::kArquebus) {
      const TroopTier &tier = unit.getCurrentTier();
      if (!tier.weaponMesh) continue;

      glm::mat4 handWorldMatrix = unit.getBoneWorldTransform("mixamorig:LeftHand");

      glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(handWorldMatrix * tier.weaponOffset));
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, tier.weaponTexture);
      glUniform1i(cachedUniformLocation(objShader, "tex"), 0);
      tier.weaponMesh->draw();

    } else if (unit.type == defender_types::kCannon) {
      // O canhoneiro é renderizado deslocado para trás do ponto lógico; o bone
      // deve ser calculado a partir dessa mesma posição deslocada.
      const float behind = game_constants::kCannonerBehindOffset;
      const float bkX =  std::sin(unit.rotationY);
      const float bkZ = -std::cos(unit.rotationY);
      const float ox = unit.position[0];
      const float oz = unit.position[2];
      unit.position[0] += bkX * behind;
      unit.position[2] += bkZ * behind;

      glm::mat4 handWorldMatrix = unit.getBoneWorldTransform("mixamorig:LeftHand");

      unit.position[0] = ox;
      unit.position[2] = oz;

      glm::mat4 torchOffset = glm::mat4(0.0f);
      const float ts = 8.0f;
      torchOffset[0][1] = ts;   // torch X → hand Y (forward)
      torchOffset[1][0] = -ts;  // torch Y → hand -X (world up)
      torchOffset[2][2] = ts;   // torch Z → hand Z (left)
      torchOffset[3][3] = 1.0f;

      glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(handWorldMatrix * torchOffset));
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, torchTexture);
      glUniform1i(cachedUniformLocation(objShader, "tex"), 0);
      torchMesh.draw();
    }
  }
}

// =============================================================================
// CANHÕES (barris estáticos posicionados à frente de cada canhoneiro)
// =============================================================================

void renderCannonBarrels(GLuint objShader,
                         const ObjUniforms &u,
                         const std::vector<GameObject> &defenders,
                         Mesh &cannonBarrelMesh,
                         unsigned int cannonBarrelTex) {
  using game_constants::kCannonBarrelOffset;
  using game_constants::kCannonBarrelScale;

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, cannonBarrelTex);
  glUniform1i(cachedUniformLocation(objShader, "tex"), 0);

  for (const auto &unit : defenders) {
    if (unit.type != defender_types::kCannon) continue;

    // Forward = (-sin(rotY), 0, cos(rotY)) — mesma convenção que GameObject::draw usa
    // (draw aplica rotateY(-rotationY), cujo +Z em world-space é exatamente esse vetor).
    float fwdX = -std::sin(unit.rotationY);
    float fwdZ =  std::cos(unit.rotationY);
    float bx = unit.position[0] + fwdX * kCannonBarrelOffset;
    float by = unit.position[1];
    float bz = unit.position[2] + fwdZ * kCannonBarrelOffset;

    Matrix<4, 4> mt = translate<4, 4>(bx, by, bz);
    Matrix<4, 4> mr = rotateY<4, 4>(math_constants::kPi - unit.rotationY);  // inverte muzzle
    Matrix<4, 4> ms = scale<4, 4>(kCannonBarrelScale, kCannonBarrelScale, kCannonBarrelScale);
    auto gl = toOpenGLMatrix(mt * mr * ms);
    glUniformMatrix4fv(u.model, 1, GL_FALSE, gl.data());
    cannonBarrelMesh.draw();
  }
}

// =============================================================================
// CASTELO
// =============================================================================

void renderCastle(GLuint objShader,
                  const ObjUniforms &u,
                  const std::vector<CastleGroup> &groups,
                  const std::vector<Point> &curvePoints) {
  if (curvePoints.empty() || groups.empty()) return;

  Point endPoint = curvePoints.back();
  const float offsetX    = 16.0f;
  const float offsetZ    = -4.0f;
  const float rotationDeg = 200.0f;
  const float uniformScale = 0.9f;

  Matrix<4, 4> t = translate<4, 4>(endPoint.x + offsetX, 0.0f, endPoint.y + offsetZ);
  Matrix<4, 4> r = rotateY<4, 4>(rotationDeg * math_constants::kDegToRad);
  Matrix<4, 4> s = scale<4, 4>(uniformScale, uniformScale, uniformScale);
  auto gl = toOpenGLMatrix(t * r * s);
  glUniformMatrix4fv(u.model, 1, GL_FALSE, gl.data());

  for (const auto &g : groups) {
    if (!g.mesh) continue;
    bindColorAndNormal(objShader, g.colorTex, g.normalTex);
    g.mesh->draw();
  }
}

// =============================================================================
// FLECHAS
// =============================================================================

void renderArrows(GLuint objShader,
                  const ObjUniforms &u,
                  const std::vector<ArrowProjectile> &arrows,
                  Mesh &arrowMesh,
                  GLuint arrowTex) {
  if (arrows.empty()) return;

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, arrowTex);
  glUniform1i(cachedUniformLocation(objShader, "tex"), 0);

  for (const auto &arrow : arrows) {
    glm::vec3 forward  = glm::normalize(arrow.velocity);
    glm::vec3 up_hint  = (std::abs(forward.y) < 0.99f)
                         ? glm::vec3(0.0f, 1.0f, 0.0f)
                         : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right    = glm::normalize(glm::cross(up_hint, forward));
    glm::vec3 up       = glm::cross(forward, right);

    glm::mat4 m(1.0f);
    m[0] = glm::vec4(forward * kArrowScale, 0.0f);  // mesh modelado no eixo X
    m[1] = glm::vec4(up      * kArrowScale, 0.0f);
    m[2] = glm::vec4(right   * kArrowScale, 0.0f);
    m[3] = glm::vec4(arrow.position,        1.0f);

    glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(m));
    arrowMesh.draw();
  }
}

// =============================================================================
// CHÃO DE GRAMA
// =============================================================================

void renderGround(const GpuMesh &grassMesh,
                  const GrassTextures &textures,
                  unsigned int noiseTexture) {
  bindTextureChain({textures.color, noiseTexture, textures.normal, textures.ao,
                    textures.roughness, textures.displacement});

  glBindVertexArray(grassMesh.vao);
  glDrawArrays(GL_TRIANGLES, 0, grassMesh.vertexCount);
}

// =============================================================================
// ÁRVORES (tronco + folhagem)
// =============================================================================

GLuint setupTreeInstancing(const std::vector<TreeInstance> &trees,
                           Mesh *treeLogMesh,
                           Mesh *treeLeavesMesh) {
  // Uma matriz de modelo por árvore, no MESMO formato que o caminho antigo subia
  // via uniform (toOpenGLMatrix de translate*rotateY*scale) — garante bytes
  // idênticos aos do path não-instanciado.
  std::vector<glm::mat4> instanceData;
  instanceData.reserve(trees.size());
  for (const auto &tree : trees) {
    Matrix<4, 4> m = translate<4, 4>(tree.position[0], tree.position[1], tree.position[2]) *
                     rotateY<4, 4>(tree.rotationY) *
                     scale<4, 4>(tree.scale, tree.scale, tree.scale);
    auto gl = toOpenGLMatrix(m);
    instanceData.push_back(glm::make_mat4(gl.data()));
  }

  GLuint instanceVBO = 0;
  glGenBuffers(1, &instanceVBO);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
  glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(glm::mat4),
               instanceData.empty() ? nullptr : instanceData.data(), GL_STATIC_DRAW);

  // Anexa o mesmo buffer às VAOs de tronco e folhagem: uma mat4 ocupa 4 vec4
  // (locations 3..6), avançando 1 por instância (divisor 1).
  auto attach = [&](Mesh *mesh) {
    if (!mesh) return;
    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    for (int i = 0; i < 4; ++i) {
      glEnableVertexAttribArray(3 + i);
      glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                            (void *)(static_cast<size_t>(i) * sizeof(glm::vec4)));
      glVertexAttribDivisor(3 + i, 1);
    }
    glBindVertexArray(0);
  };
  attach(treeLogMesh);
  attach(treeLeavesMesh);

  return instanceVBO;
}

void renderTrees(GLuint treeShader,
                 const std::vector<TreeInstance> &trees,
                 Mesh *treeLogMesh,
                 unsigned int treeLogTexture,
                 Mesh *treeLeavesMesh,
                 unsigned int treeLeavesTexture) {
  if (trees.empty() || (treeLogMesh == nullptr && treeLeavesMesh == nullptr)) return;

  const GLsizei count = static_cast<GLsizei>(trees.size());

  auto drawInstanced = [&](Mesh *mesh, unsigned int tex) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(cachedUniformLocation(treeShader, "tex"), 0);
    glBindVertexArray(mesh->vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh->vertices.size()), count);
    glBindVertexArray(0);
  };

  if (treeLogMesh != nullptr && treeLogTexture != 0) {
    drawInstanced(treeLogMesh, treeLogTexture);
  }

  if (treeLeavesMesh != nullptr && treeLeavesTexture != 0) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawInstanced(treeLeavesMesh, treeLeavesTexture);
    glDisable(GL_BLEND);
  }
}

// =============================================================================
// LANTERNAS
// =============================================================================

void renderLanterns(const LanternUniforms &u,
                    const std::vector<LanternInstance> &lanterns,
                    Mesh &lanternMesh,
                    const LanternTextures &textures) {
  if (lanterns.empty()) return;

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  bindTextureChain({textures.color, textures.normal, textures.roughness,
                    textures.metallic, textures.ao, textures.opacity});

  for (const auto &lantern : lanterns) {
    uploadModelMatrix(u.model, lantern.position, lantern.rotationY,
                      game_constants::kLanternScale);
    lanternMesh.draw();
  }

  glDisable(GL_CULL_FACE);
}

// =============================================================================
// CAMINHO DE TERRA
// =============================================================================

void renderPath(const PathUniforms &u,
                Mesh &pathMesh,
                const DirtTextures &textures,
                unsigned int noiseTexture,
                const Matrix<4, 4> &identity) {
  glUniformMatrix4fv(u.model, 1, GL_FALSE, identity.getData());

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);

  bindTextureChain({textures.color, noiseTexture, textures.normal, textures.ao,
                    textures.roughness, textures.displacement});

  pathMesh.draw();

  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

// =============================================================================
// CÉU NOTURNO
// =============================================================================

void renderSky(GLuint skyShader,
               GLint skyViewLoc,
               GLint skyProjLoc,
               GLint skyTexLoc,
               const GpuMesh &skyMesh,
               unsigned int skyTexture,
               const Camera &cam,
               const std::array<float, 16> &glProj) {
  glDepthFunc(GL_LEQUAL);
  glUseProgram(skyShader);

  // Remove translação da view matrix para que o céu fique "infinitamente" longe.
  Matrix<4, 4> viewMatrix = cam.getViewMatrix();
  viewMatrix(0, 3) = 0.0f;
  viewMatrix(1, 3) = 0.0f;
  viewMatrix(2, 3) = 0.0f;
  auto glSkyView = toOpenGLMatrix(viewMatrix);

  glUniformMatrix4fv(skyViewLoc, 1, GL_FALSE, glSkyView.data());
  glUniformMatrix4fv(skyProjLoc, 1, GL_FALSE, glProj.data());
  glUniform1i(skyTexLoc, 0);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, skyTexture);

  glBindVertexArray(skyMesh.vao);
  glDrawArrays(GL_TRIANGLES, 0, skyMesh.vertexCount);
  glDepthFunc(GL_LESS);
}

// =============================================================================
// CURVA DE DEBUG
// =============================================================================

void renderCurveOverlay(GLuint lineShader,
                        const LineUniforms &u,
                        const GpuMesh &curveMesh,
                        const std::array<float, 16> &glView,
                        const std::array<float, 16> &glProj) {
  static const float kIdentity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  glUseProgram(lineShader);
  glUniformMatrix4fv(u.view,       1, GL_FALSE, glView.data());
  glUniformMatrix4fv(u.projection, 1, GL_FALSE, glProj.data());
  glUniformMatrix4fv(u.model,      1, GL_FALSE, kIdentity);
  glUniform4f(u.color, 1.0f, 1.0f, 0.0f, 1.0f);  // yellow
  glBindVertexArray(curveMesh.vao);
  glDrawArrays(GL_LINE_STRIP, 0, curveMesh.vertexCount);
}
