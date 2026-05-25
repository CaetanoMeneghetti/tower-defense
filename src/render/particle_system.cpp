#include "render/particle_system.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <glm/gtc/type_ptr.hpp>

namespace {

static const float kQuad[] = {
  // pos (x,y,z)    uv (u,v)
  -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
   0.5f, -0.5f, 0.0f,   1.0f, 0.0f,
   0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
  -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
   0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
  -0.5f,  0.5f, 0.0f,   0.0f, 1.0f,
};

}  // namespace

CannonSmoke::CannonSmoke() {
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void*)(3 * sizeof(float)));
  glBindVertexArray(0);
}

CannonSmoke::~CannonSmoke() {
  glDeleteVertexArrays(1, &vao_);
  glDeleteBuffers(1, &vbo_);
}

void CannonSmoke::emit(const glm::vec3 &origin, int count, float sizeScale) {
  for (int i = 0; i < count; ++i) {
    SmokeParticle p;
    p.position = origin;
    float angle = (std::rand() % 360) * (3.14159f / 180.0f);
    float radial = 0.05f + (std::rand() % 100) * 0.002f;
    p.velocity = glm::vec3(
        std::cos(angle) * radial,
        0.5f + (std::rand() % 100) * 0.004f,
        std::sin(angle) * radial);
    p.lifetime = 1.2f + (std::rand() % 100) * 0.008f;
    p.size     = (0.2f + (std::rand() % 100) * 0.003f) * sizeScale;
    p.age      = 0.0f;
    particles_.push_back(p);
  }
}

void CannonSmoke::update(float deltaTime) {
  for (auto &p : particles_) {
    p.age      += deltaTime;
    p.position += p.velocity * deltaTime;
    p.size     += 0.3f * deltaTime;
  }
  particles_.erase(
      std::remove_if(particles_.begin(), particles_.end(),
                     [](const SmokeParticle &p) { return p.age >= p.lifetime; }),
      particles_.end());
}

void CannonSmoke::render(GLuint shader,
                         unsigned int texture,
                         const glm::vec3 &camRight,
                         const glm::vec3 &camUp,
                         const std::array<float, 16> &glView,
                         const std::array<float, 16> &glProj) {
  if (particles_.empty()) return;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);

  glUseProgram(shader);
  glUniformMatrix4fv(glGetUniformLocation(shader, "view"),       1, GL_FALSE, glView.data());
  glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glProj.data());
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glUniform1i(glGetUniformLocation(shader, "tex"), 0);

  const GLint modelLoc = glGetUniformLocation(shader, "model");
  const GLint alphaLoc = glGetUniformLocation(shader, "alpha");

  glBindVertexArray(vao_);
  for (const auto &p : particles_) {
    float t     = p.age / p.lifetime;
    float alpha = (1.0f - t) * 0.65f;

    glm::mat4 model(0.0f);
    model[0] = glm::vec4(camRight * p.size, 0.0f);
    model[1] = glm::vec4(camUp    * p.size, 0.0f);
    model[2] = glm::vec4(glm::cross(camRight, camUp) * p.size, 0.0f);
    model[3] = glm::vec4(p.position, 1.0f);

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1f(alphaLoc, alpha);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }
  glBindVertexArray(0);

  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}
