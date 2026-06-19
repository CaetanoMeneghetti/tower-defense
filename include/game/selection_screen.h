#pragma once

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "engine/animated_model.h"
#include "engine/camera.h"
#include "engine/lighting.h"
#include "engine/mesh.h"
#include "math/vector.h"
#include "render/shader_uniforms.h"

// =============================================================================
// SELECTION SCREEN
// =============================================================================

struct EmberParticle {
  glm::vec3 position;
  glm::vec3 velocity;
  float age      = 0.0f;
  float lifetime = 20.0f;
  float size     = 0.06f;
};

class SelectionScreen {
 public:
  bool run(GLFWwindow *window);
  int  selectedGeneral() const { return selectedGeneral_; }

 private:
  // --- shaders ---------------------------------------------------------------
  GLuint      objShader_      = 0;
  GLuint      animShader_     = 0;
  GLuint      particleShader_ = 0;
  ObjUniforms objU_{};

  // --- environment -----------------------------------------------------------
  std::unique_ptr<Mesh> wallMesh_;   // used twice: floor (rotated) + back wall
  GLuint floorTex_         = 0;
  GLuint wallTex_          = 0;
  GLuint defaultNormalTex_ = 0;

  // --- weapons ---------------------------------------------------------------
  std::unique_ptr<Mesh> bowMesh_;
  std::unique_ptr<Mesh> swordMesh_;
  GLuint bowTex_   = 0;
  GLuint swordTex_ = 0;

  // --- banners (one mesh + texture per faction) ------------------------------
  std::unique_ptr<Mesh> bannerMesh_[3];   // 0=English 1=Byzantine 2=Turk
  GLuint bannerTex_[3] = {};

  // --- tunable scene scale / position (driven by ImGui sliders) -------------
  float floorSX_      =  8.0f;
  float floorSY_      =  8.0f;   // controls depth after rotateX(-pi/2)
  float wallSX_       = 10.0f;
  float wallSY_       =  8.0f;
  float wallZ_        = -4.0f;
  float bannerScale_  =  1.0f;
  float bannerZOffset_= -0.4f;   // how far behind each general

  // --- generals (0=English 1=Byzantine 2=Turk) -------------------------------
  std::unique_ptr<AnimatedModel> generals_[3];
  GLuint    generalTex_[3]      = {};
  float     generalAnimTime_[3] = {};
  glm::vec3 generalPos_[3];

  // --- selection state -------------------------------------------------------
  int  selectedGeneral_ = 0;
  bool leftWasDown_     = false;
  bool rightWasDown_    = false;

  // --- ember particles -------------------------------------------------------
  std::vector<EmberParticle> embers_;
  GLuint emberVao_        = 0;
  GLuint emberVbo_        = 0;
  GLuint emberTex_        = 0;
  float  emberSpawnAccum_ = 0.0f;

  // --- fixed camera ----------------------------------------------------------
  glm::vec3 camPos_   = {};
  float     camYaw_   = 0.0f;
  float     camPitch_ = 0.0f;

  // --- lighting + camera -----------------------------------------------------
  DirectionalLight moonLight_;
  Camera           cam_;
  float            sceneTime_ = 0.0f;

  void loadScene();
  void updateScene(float dt);
  void renderScene(int fbW, int fbH);
  void unloadScene();
  void renderUI(float W, float H, bool &outBegin, bool &outBack);
};
