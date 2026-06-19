#pragma once

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "engine/animated_model.h"
#include "engine/camera.h"
#include "engine/lighting.h"
#include "engine/primitive_meshes.h"
#include "math/vector.h"
#include "render/scene_renderer.h"
#include "render/shader_uniforms.h"

// =============================================================================
// MAIN MENU — first screen shown on startup.
//
// Flow: MainMenu → SelectionScreen → Game
//
// run() returns true  → proceed to SelectionScreen
//               false → quit application
//
// To extend the 3D scene, add members below and implement the four hooks
// in main_menu.cpp. The current scene renders: ground plane + animated grass.
// =============================================================================

class MainMenu {
 public:
  bool run(GLFWwindow *window);

 private:
  // =========================================================================
  // 3D SCENE — add your own models, shaders, textures here.
  // =========================================================================

  // --- ground ---------------------------------------------------------------
  GLuint         groundShader_ = 0;
  GroundUniforms groundU_{};
  GpuMesh        groundMesh_;
  GrassTextures  grassTex_{};
  GLuint         noiseTex_     = 0;

  // --- animated grass model -------------------------------------------------
  GLuint                        animShader_    = 0;
  std::unique_ptr<AnimatedModel> grassModel_;
  GLuint                        grassColorTex_ = 0;
  GLuint                        grassNormalTex_= 0;
  std::vector<glm::vec3>        grassPositions_;

  // --- lighting + camera ----------------------------------------------------
  DirectionalLight moonLight_;
  Camera           cam_;

  float sceneTime_ = 0.0f;

  // ---- Scene lifecycle — implement in main_menu.cpp -----------------------
  void loadScene();
  void updateScene(float dt);
  void renderScene(int fbW, int fbH);
  void unloadScene();

  // ---- ImGui overlay — implement in main_menu.cpp -------------------------
  void renderUI(float W, float H, bool &outEnter, bool &outQuit);
};
