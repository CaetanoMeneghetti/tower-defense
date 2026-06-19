#include "render/explosion_system.h"

#include <algorithm>

#include <glm/gtc/type_ptr.hpp>

// Quad unitário: 2 triângulos, posição XY local (-0.5..0.5) + UV (0..1)
static const float kQuadVerts[] = {
    // x      y     u     v
    -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  1.0f, 1.0f,

    -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.0f, 1.0f,
};

void initExplosionSystem(ExplosionSystem &sys, GLuint shader, GLuint texture) {
  sys.shader  = shader;
  sys.texture = texture;

  glGenVertexArrays(1, &sys.vao);
  glGenBuffers(1, &sys.vbo);

  glBindVertexArray(sys.vao);
  glBindBuffer(GL_ARRAY_BUFFER, sys.vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, GL_STATIC_DRAW);

  // location 0: posição XY (stride = 4 floats)
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  // location 1: UV
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

  glBindVertexArray(0);
}

void spawnExplosion(ExplosionSystem &sys, const glm::vec3 &position) {
  sys.active.push_back({position, 0.0f});
}

void updateExplosions(ExplosionSystem &sys, float deltaTime) {
  for (auto &e : sys.active) e.elapsed += deltaTime;
  sys.active.erase(
      std::remove_if(sys.active.begin(), sys.active.end(),
                     [](const ExplosionInstance &e) {
                       return e.elapsed >= kExplosionDuration;
                     }),
      sys.active.end());
}

void renderExplosions(ExplosionSystem &sys,
                      const glm::vec3 &camRight,
                      const glm::vec3 &camUp,
                      const glm::mat4 &view,
                      const glm::mat4 &proj) {
  if (sys.active.empty()) return;

  glUseProgram(sys.shader);
  glUniformMatrix4fv(glGetUniformLocation(sys.shader, "view"),       1, GL_FALSE, glm::value_ptr(view));
  glUniformMatrix4fv(glGetUniformLocation(sys.shader, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
  glUniform3fv(glGetUniformLocation(sys.shader, "camRight"), 1, glm::value_ptr(camRight));
  glUniform3fv(glGetUniformLocation(sys.shader, "camUp"),    1, glm::value_ptr(camUp));
  glUniform1f(glGetUniformLocation(sys.shader, "scale"),     kExplosionScale);
  glUniform1i(glGetUniformLocation(sys.shader, "tex"),       0);

  const float frameW = 1.0f / kExplosionCols;
  const float frameH = 1.0f / kExplosionRows;
  glUniform2f(glGetUniformLocation(sys.shader, "uvSize"), frameW, frameH);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sys.texture);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);  // não escreve no depth buffer (evita z-fighting)

  glBindVertexArray(sys.vao);

  const int   centerLoc    = glGetUniformLocation(sys.shader, "center");
  const int   uvOffsetLoc  = glGetUniformLocation(sys.shader, "uvOffset");
  const int   alphaLoc     = glGetUniformLocation(sys.shader, "alpha");

  for (const auto &e : sys.active) {
    float t     = e.elapsed / kExplosionDuration;
    int   frame = static_cast<int>(t * kExplosionFrames);
    if (frame >= kExplosionFrames) frame = kExplosionFrames - 1;

    int col = frame % kExplosionCols;
    int row = frame / kExplosionCols;

    // UV origin: spritesheet Y=0 é o topo, OpenGL Y=0 é a base — invertemos a row
    float u = col * frameW;
    float v = (kExplosionRows - 1 - row) * frameH;

    // Fade suave no último quarto da animação
    float alpha = (t > 0.75f) ? (1.0f - (t - 0.75f) / 0.25f) : 1.0f;

    glUniform2f(uvOffsetLoc, u, v);
    glUniform3fv(centerLoc, 1, glm::value_ptr(e.position));
    glUniform1f(alphaLoc, alpha);

    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  glBindVertexArray(0);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void destroyExplosionSystem(ExplosionSystem &sys) {
  glDeleteVertexArrays(1, &sys.vao);
  glDeleteBuffers(1, &sys.vbo);
}
