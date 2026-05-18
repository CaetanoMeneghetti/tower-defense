#include "render/scene_renderer.h"

#include <glm/gtc/type_ptr.hpp>
#include <string>

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
  glUniform1i(glGetUniformLocation(shader, "tex"), 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, normal);
  glUniform1i(glGetUniformLocation(shader, "normalMap"), 1);
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
  glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glView.data());
  glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glProj.data());
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
  if (!enemy.alive) {
    glUniform1f(glGetUniformLocation(shaderAnim, "hitFlash"), 0.0f);
    return;
  }

  bindColorAndNormal(shaderAnim, enemyTexture, defaultNormal);

  enemyModel.position = enemyPos;
  enemyModel.rotationY = enemyAngle;

  const float flash = (game_constants::kEnemyHitFlashDuration > 0.0f)
                          ? (enemy.hitFlashTime / game_constants::kEnemyHitFlashDuration)
                          : 0.0f;
  glUniform1f(glGetUniformLocation(shaderAnim, "hitFlash"), flash);
  enemyModel.draw(shaderAnim);

  // Reseta pra nao vazar flash para os defensores no mesmo shader.
  glUniform1f(glGetUniformLocation(shaderAnim, "hitFlash"), 0.0f);
}

// =============================================================================
// DEFENSORES (corpo animado)
// =============================================================================

void renderDefenders(GLuint shaderAnim,
                     std::vector<GameObject> &defenders,
                     unsigned int archerTex,
                     unsigned int archerNormal) {
  // Arqueiro: textura/normal fixos (todos os tiers compartilham hoje).
  // Arcabuz: pega do tier atual — abre espaço para tiers futuros com look próprio.
  for (auto &unit : defenders) {
    if (unit.type == 1) {
      bindColorAndNormal(shaderAnim, archerTex, archerNormal);
    } else if (unit.type == 2) {
      const TroopTier &tier = unit.getCurrentTier();
      bindColorAndNormal(shaderAnim, tier.texture, tier.normalMap);
    }
    unit.draw(shaderAnim);
  }
}

// =============================================================================
// ARMAS DOS DEFENSORES (anexadas ao bone da mão)
// =============================================================================

void renderDefenderWeapons(GLuint objShader,
                           const ObjUniforms &u,
                           std::vector<GameObject> &defenders,
                           Mesh &bowMesh,
                           unsigned int bowTexture) {
  // Arqueiro: arco fixo + offset cinemático hardcoded (igual nos 5 tiers).
  // Arcabuz: mesh/textura/offset vêm do tier atual (ver game/upgrade_system.h).
  for (auto &unit : defenders) {
    if (unit.type == 1) {
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
      glm::mat4 finalBowMatrix = handWorldMatrix * offset;

      glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(finalBowMatrix));
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, bowTexture);
      glUniform1i(glGetUniformLocation(objShader, "tex"), 0);
      bowMesh.draw();
    } else if (unit.type == 2) {
      const TroopTier &tier = unit.getCurrentTier();
      if (!tier.weaponMesh) continue;

      glm::mat4 handWorldMatrix = unit.getBoneWorldTransform("mixamorig:LeftHand");
      glm::mat4 finalWeaponMatrix = handWorldMatrix * tier.weaponOffset;

      glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(finalWeaponMatrix));
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, tier.weaponTexture);
      glUniform1i(glGetUniformLocation(objShader, "tex"), 0);
      tier.weaponMesh->draw();
    }
  }
}

// =============================================================================
// CASTELO
// =============================================================================

void renderCastle(GLuint objShader,
                  const ObjUniforms &u,
                  Mesh &castleMesh,
                  unsigned int castleTex,
                  unsigned int castleNormal,
                  const std::vector<Point> &curvePoints) {
  if (curvePoints.empty()) return;

  // Posiciona o castelo no fim da curva com offset manual e rotação angular.
  Point endPoint = curvePoints.back();
  const float offsetX = 4.0f;
  const float offsetZ = 4.0f;
  const float rotationDeg = -55.0f;
  const float uniformScale = 0.072f;

  Matrix<4, 4> t = translate<4, 4>(endPoint.x + offsetX, 0.0f, endPoint.y + offsetZ);
  Matrix<4, 4> r = rotateY<4, 4>(rotationDeg * math_constants::kDegToRad);
  Matrix<4, 4> s = scale<4, 4>(uniformScale, uniformScale, uniformScale);
  Matrix<4, 4> model = t * r * s;
  auto gl = toOpenGLMatrix(model);

  glUniformMatrix4fv(u.model, 1, GL_FALSE, gl.data());
  bindColorAndNormal(objShader, castleTex, castleNormal);
  castleMesh.draw();
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

void renderTrees(GLuint objShader,
                 const ObjUniforms &u,
                 const std::vector<TreeInstance> &trees,
                 Mesh *treeLogMesh,
                 unsigned int treeLogTexture,
                 Mesh *treeLeavesMesh,
                 unsigned int treeLeavesTexture) {
  if (trees.empty() || (treeLogMesh == nullptr && treeLeavesMesh == nullptr)) return;

  auto drawInstances = [&](Mesh *mesh, unsigned int tex) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(objShader, "tex"), 0);
    for (const auto &tree : trees) {
      uploadModelMatrix(u.model, tree.position, tree.rotationY, tree.scale);
      mesh->draw();
    }
  };

  if (treeLogMesh != nullptr && treeLogTexture != 0) {
    drawInstances(treeLogMesh, treeLogTexture);
  }

  if (treeLeavesMesh != nullptr && treeLeavesTexture != 0) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawInstances(treeLeavesMesh, treeLeavesTexture);
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
  glUseProgram(lineShader);
  glUniformMatrix4fv(u.view, 1, GL_FALSE, glView.data());
  glUniformMatrix4fv(u.projection, 1, GL_FALSE, glProj.data());
  glBindVertexArray(curveMesh.vao);
  glDrawArrays(GL_LINE_STRIP, 0, curveMesh.vertexCount);
}
