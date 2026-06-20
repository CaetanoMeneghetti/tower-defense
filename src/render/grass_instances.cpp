#include "render/grass_instances.h"

#include <cmath>
#include <random>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "game/path_navigation.h"
#include "math/constants.h"

GrassField buildGrassField(GLuint shader, GLuint texture,
                           const std::vector<Point> &curvePoints,
                           float pathClearance,
                           float areaHalfX, float areaHalfZ,
                           float spacing, float baseScale) {
  GrassField field;
  field.shader  = shader;
  field.texture = texture;

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> jitterDist(-spacing * 0.45f, spacing * 0.45f);
  std::uniform_real_distribution<float> rotDist(0.0f, math_constants::kTwoPi);
  std::uniform_real_distribution<float> scaleDist(0.55f, 0.85f);
  std::uniform_real_distribution<float> keepDist(0.0f, 1.0f);

  const int stepsX = static_cast<int>(areaHalfX / spacing);
  const int stepsZ = static_cast<int>(areaHalfZ / spacing);

  // Densidade da grama cai com a distância ao centro do mapa (0,0): cheia só no
  // núcleo (24% do raio máximo) e decai de forma QUADRÁTICA até zero no canto mais
  // distante. O decaimento quadrático mantém o núcleo denso igual ao de antes mas
  // deixa a borda bem esparsa, então a grama alcança bem mais longe sem explodir a
  // contagem de instâncias (instancing desenha todas) — preservando o FPS.
  const float maxRadius  = std::sqrt(areaHalfX * areaHalfX + areaHalfZ * areaHalfZ);
  const float fullRadius = maxRadius * 0.24f;
  const float fallSpan   = maxRadius - fullRadius;

  std::vector<glm::mat4> matrices;
  std::vector<glm::mat3> normalMatrices;
  matrices.reserve((2 * stepsX + 1) * (2 * stepsZ + 1));
  normalMatrices.reserve(matrices.capacity());

  for (int i = -stepsX; i <= stepsX; ++i) {
    for (int j = -stepsZ; j <= stepsZ; ++j) {
      float x = i * spacing + jitterDist(rng);
      float z = j * spacing + jitterDist(rng);
      if (distanceToPath(curvePoints, x, z) < pathClearance) continue;

      // Redução gradual de densidade conforme se afasta do centro do mapa.
      // Queda quadrática: linear² faz a borda decair mais rápido que o núcleo.
      const float radius = std::sqrt(x * x + z * z);
      float keepProb = 1.0f;
      if (radius > fullRadius) {
        const float linear = 1.0f - (radius - fullRadius) / fallSpan;
        keepProb = linear > 0.0f ? linear * linear : 0.0f;
      }
      if (keepProb <= 0.0f || keepDist(rng) > keepProb) continue;

      float s = baseScale * scaleDist(rng);
      float r = rotDist(rng);
      glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z));
      m = glm::rotate(m, r, glm::vec3(0.0f, 1.0f, 0.0f));
      m = glm::scale(m, glm::vec3(s));
      matrices.push_back(m);
      normalMatrices.push_back(glm::mat3(glm::transpose(glm::inverse(m))));
    }
  }

  field.instanceCount = static_cast<int>(matrices.size());

  glGenBuffers(1, &field.instanceVBO);
  glBindBuffer(GL_ARRAY_BUFFER, field.instanceVBO);
  glBufferData(GL_ARRAY_BUFFER,
               field.instanceCount * sizeof(glm::mat4),
               matrices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &field.normalVBO);
  glBindBuffer(GL_ARRAY_BUFFER, field.normalVBO);
  glBufferData(GL_ARRAY_BUFFER,
               field.instanceCount * sizeof(glm::mat3),
               normalMatrices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  return field;
}

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
                      const glm::mat4  &proj) {
  glUseProgram(field.shader);

  glUniform1f(glGetUniformLocation(field.shader, "time"),         time);
  glUniformMatrix4fv(glGetUniformLocation(field.shader, "view"),       1, GL_FALSE, glm::value_ptr(view));
  glUniformMatrix4fv(glGetUniformLocation(field.shader, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
  glUniform3fv(glGetUniformLocation(field.shader, "lightDir"),     1, glm::value_ptr(lightDir));
  glUniform3fv(glGetUniformLocation(field.shader, "lightAmbient"), 1, glm::value_ptr(lightAmbient));
  glUniform3fv(glGetUniformLocation(field.shader, "lightDiffuse"), 1, glm::value_ptr(lightDiffuse));
  glUniform3fv(glGetUniformLocation(field.shader, "fogColor"),     1, glm::value_ptr(fogColor));
  glUniform1f(glGetUniformLocation(field.shader, "fogStart"),      fogStart);
  glUniform1f(glGetUniformLocation(field.shader, "fogEnd"),        fogEnd);
  glUniform3fv(glGetUniformLocation(field.shader, "viewPos"),      1, glm::value_ptr(viewPos));

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, field.texture);
  glUniform1i(glGetUniformLocation(field.shader, "tex"), 0);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  model.drawInstanced(field.instanceCount);

  glDisable(GL_BLEND);
}
