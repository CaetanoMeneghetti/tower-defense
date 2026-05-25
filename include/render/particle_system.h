#pragma once

#include <vector>
#include <array>

#include <glad/glad.h>
#include <glm/glm.hpp>

struct SmokeParticle {
  glm::vec3 position;
  glm::vec3 velocity;
  float age      = 0.0f;
  float lifetime = 2.0f;
  float size     = 0.3f;
};

class CannonSmoke {
 public:
  CannonSmoke();
  ~CannonSmoke();

  void emit(const glm::vec3 &origin, int count = 10, float sizeScale = 1.0f);
  void update(float deltaTime);
  void render(GLuint shader,
              unsigned int texture,
              const glm::vec3 &camRight,
              const glm::vec3 &camUp,
              const std::array<float, 16> &glView,
              const std::array<float, 16> &glProj);

 private:
  std::vector<SmokeParticle> particles_;
  GLuint vao_ = 0;
  GLuint vbo_ = 0;
};
