#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

// =============================================================================
// EXPLOSÃO BILLBOARD — spritesheet 4×4, 16 frames, sempre virado pra câmera
// =============================================================================

constexpr int   kExplosionCols     = 4;
constexpr int   kExplosionRows     = 4;
constexpr int   kExplosionFrames   = kExplosionCols * kExplosionRows;
constexpr float kExplosionDuration = 0.5f;   // segundos para passar os 16 frames
constexpr float kExplosionScale    = 4.0f;   // tamanho do quad no mundo

struct ExplosionInstance {
  glm::vec3 position;
  float     elapsed = 0.0f;
};

struct ExplosionSystem {
  std::vector<ExplosionInstance> active;
  GLuint vao     = 0;
  GLuint vbo     = 0;
  GLuint shader  = 0;
  GLuint texture = 0;
};

void initExplosionSystem(ExplosionSystem &sys, GLuint shader, GLuint texture);
void spawnExplosion(ExplosionSystem &sys, const glm::vec3 &position);
void updateExplosions(ExplosionSystem &sys, float deltaTime);
void renderExplosions(ExplosionSystem       &sys,
                      const glm::vec3       &camRight,
                      const glm::vec3       &camUp,
                      const glm::mat4       &view,
                      const glm::mat4       &proj);
void destroyExplosionSystem(ExplosionSystem &sys);
