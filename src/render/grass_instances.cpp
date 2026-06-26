#include "render/grass_instances.h"

#include <cmath>
#include <random>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "engine/lighting.h"
#include "engine/obj_loader.h"
#include "game/path_navigation.h"
#include "math/constants.h"
#include "math/matrix_ops.h"
#include "math/opengl_utils.h"
#include "math/transforms.h"

GrassField buildGrassField(GLuint shader, GLuint texture,
                           const std::vector<Point> &curvePoints,
                           float pathClearance,
                           float areaHalfX, float areaHalfZ,
                           float spacing, float baseScale) {
  GrassField field;
  field.shader  = shader;
  field.texture = texture;

  // ---- Carregar mesh do grass.glb via assimp ----
  std::vector<Vertex> verts;
  {
    Assimp::Importer imp;
    const aiScene *sc = imp.ReadFile("data/models/world/grass.glb",
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);
    if (sc && !(sc->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && sc->mRootNode) {
      for (unsigned int m = 0; m < sc->mNumMeshes; ++m) {
        aiMesh *mesh = sc->mMeshes[m];
        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
          const aiFace &face = mesh->mFaces[f];
          for (unsigned int fi = 0; fi < face.mNumIndices; ++fi) {
            unsigned int idx = face.mIndices[fi];
            Vertex v;
            v.position  = { mesh->mVertices[idx].x, mesh->mVertices[idx].y, mesh->mVertices[idx].z };
            v.texcoords = mesh->mTextureCoords[0]
                ? Vec2{ mesh->mTextureCoords[0][idx].x, mesh->mTextureCoords[0][idx].y }
                : Vec2{ 0.0f, 0.0f };
            v.normal    = mesh->mNormals
                ? Vec3{ mesh->mNormals[idx].x, mesh->mNormals[idx].y, mesh->mNormals[idx].z }
                : Vec3{ 0.0f, 1.0f, 0.0f };
            verts.push_back(v);
          }
        }
      }
    }
  }
  field.vertexCount = static_cast<int>(verts.size());

  glGenVertexArrays(1, &field.meshVAO);
  glGenBuffers(1, &field.meshVBO);
  glBindVertexArray(field.meshVAO);

  glBindBuffer(GL_ARRAY_BUFFER, field.meshVBO);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void*)offsetof(Vertex, position));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void*)offsetof(Vertex, texcoords));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void*)offsetof(Vertex, normal));

  // ---- Gerar matrizes de instância (mesmo padrão do stash: grid simples) ----
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> jitterDist(-spacing * 0.45f, spacing * 0.45f);
  std::uniform_real_distribution<float> rotDist(0.0f, math_constants::kTwoPi);
  std::uniform_real_distribution<float> scaleDist(0.55f, 0.85f);

  const int stepsX = static_cast<int>(areaHalfX / spacing);
  const int stepsZ = static_cast<int>(areaHalfZ / spacing);

  std::vector<glm::mat4> matrices;
  std::vector<glm::mat3> normalMatrices;
  matrices.reserve((2 * stepsX + 1) * (2 * stepsZ + 1));
  normalMatrices.reserve(matrices.capacity());

  for (int i = -stepsX; i <= stepsX; ++i) {
    for (int j = -stepsZ; j <= stepsZ; ++j) {
      float x = i * spacing + jitterDist(rng);
      float z = j * spacing + jitterDist(rng);
      if (distanceToPath(curvePoints, x, z) < pathClearance) continue;

      float s = baseScale * scaleDist(rng);
      float r = rotDist(rng);

      // Model matrix com a biblioteca de matrizes própria (math/), mesmo caminho
      // usado para as árvores em setupTreeInstancing: m = T * Ry(r) * S(s).
      // toOpenGLMatrix transpõe para o layout coluna-maior que o OpenGL espera.
      Matrix<4, 4> m = translate<4, 4>(x, 0.0f, z) *
                       rotateY<4, 4>(r) *
                       scale<4, 4>(s, s, s);
      glm::mat4 glModel = glm::make_mat4(toOpenGLMatrix(m).data());
      matrices.push_back(glModel);
      normalMatrices.push_back(glm::mat3(glm::transpose(glm::inverse(glModel))));
    }
  }

  field.instanceCount = static_cast<int>(matrices.size());

  glGenBuffers(1, &field.instanceVBO);
  glBindBuffer(GL_ARRAY_BUFFER, field.instanceVBO);
  glBufferData(GL_ARRAY_BUFFER, field.instanceCount * sizeof(glm::mat4),
               matrices.data(), GL_STATIC_DRAW);
  for (int k = 0; k < 4; ++k) {
    glEnableVertexAttribArray(3 + k);
    glVertexAttribPointer(3 + k, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                          (void*)(k * sizeof(glm::vec4)));
    glVertexAttribDivisor(3 + k, 1);
  }

  glGenBuffers(1, &field.normalVBO);
  glBindBuffer(GL_ARRAY_BUFFER, field.normalVBO);
  glBufferData(GL_ARRAY_BUFFER, field.instanceCount * sizeof(glm::mat3),
               normalMatrices.data(), GL_STATIC_DRAW);
  for (int k = 0; k < 3; ++k) {
    glEnableVertexAttribArray(7 + k);
    glVertexAttribPointer(7 + k, 3, GL_FLOAT, GL_FALSE, sizeof(glm::mat3),
                          (void*)(k * sizeof(glm::vec3)));
    glVertexAttribDivisor(7 + k, 1);
  }

  glBindVertexArray(0);
  return field;
}

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
                      const std::vector<PointLight> &pointLights) {
  if (field.instanceCount == 0 || field.vertexCount == 0) return;

  glUseProgram(field.shader);
  glUniform1f(glGetUniformLocation(field.shader, "time"),            time);
  glUniformMatrix4fv(glGetUniformLocation(field.shader, "view"),       1, GL_FALSE, glm::value_ptr(view));
  glUniformMatrix4fv(glGetUniformLocation(field.shader, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
  glUniform3fv(glGetUniformLocation(field.shader, "lightDir"),       1, glm::value_ptr(lightDir));
  glUniform3fv(glGetUniformLocation(field.shader, "lightAmbient"),   1, glm::value_ptr(lightAmbient));
  glUniform3fv(glGetUniformLocation(field.shader, "lightDiffuse"),   1, glm::value_ptr(lightDiffuse));
  glUniform3fv(glGetUniformLocation(field.shader, "fogColor"),       1, glm::value_ptr(fogColor));
  glUniform1f(glGetUniformLocation(field.shader, "fogStart"),        fogStart);
  glUniform1f(glGetUniformLocation(field.shader, "fogEnd"),          fogEnd);
  glUniform3fv(glGetUniformLocation(field.shader, "viewPos"),        1, glm::value_ptr(viewPos));
  applyPointLights(field.shader, pointLights);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, field.texture);
  glUniform1i(glGetUniformLocation(field.shader, "tex"), 0);

  glBindVertexArray(field.meshVAO);
  glDrawArraysInstanced(GL_TRIANGLES, 0, field.vertexCount, field.instanceCount);
  glBindVertexArray(0);
}
