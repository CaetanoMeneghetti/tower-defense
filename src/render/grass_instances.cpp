#include "render/grass_instances.h"

#include <cmath>
#include <random>

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

  std::mt19937 rng(42);  // seed fixa = layout determinístico
  std::uniform_real_distribution<float> jitterDist(-spacing * 0.45f, spacing * 0.45f);
  std::uniform_real_distribution<float> rotDist(0.0f, math_constants::kTwoPi);
  std::uniform_real_distribution<float> scaleDist(0.55f, 0.85f);  // escala pequena

  const int stepsX = static_cast<int>(areaHalfX / spacing);
  const int stepsZ = static_cast<int>(areaHalfZ / spacing);

  for (int i = -stepsX; i <= stepsX; ++i) {
    for (int j = -stepsZ; j <= stepsZ; ++j) {
      float x = i * spacing + jitterDist(rng);
      float z = j * spacing + jitterDist(rng);

      if (distanceToPath(curvePoints, x, z) < pathClearance) continue;

      GrassInstance inst;
      inst.position  = {x, 0.0f, z};
      inst.rotationY = rotDist(rng);
      inst.scale     = baseScale * scaleDist(rng);
      field.instances.push_back(inst);
    }
  }
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

  const int modelLoc = glGetUniformLocation(field.shader, "model");
  for (const auto &inst : field.instances) {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), inst.position);
    m = glm::rotate(m, inst.rotationY, glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::scale(m, glm::vec3(inst.scale));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(m));
    model.draw(field.shader);
  }

  glDisable(GL_BLEND);
}
