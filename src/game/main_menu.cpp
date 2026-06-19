#include "game/main_menu.h"

#include <cmath>
#include <string>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "engine/shader_program.h"
#include "engine/texture_loader.h"
#include "math/constants.h"
#include "math/matrix_ops.h"
#include "math/opengl_utils.h"
#include "math/transforms.h"
#include "render/render_constants.h"

// =============================================================================
// SCENE HELPERS
// =============================================================================

namespace {

// Draws all grass instances with the anim shader already bound and
// uploadCommonUniforms() already called for this shader.
void drawGrassInstances(GLuint shader,
                        AnimatedModel *model,
                        const std::vector<glm::vec3> &positions,
                        GLuint colorTex,
                        GLuint normalTex,
                        float  time) {
  if (!model || positions.empty()) return;

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, colorTex);
  glUniform1i(glGetUniformLocation(shader, "tex"), 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, normalTex);
  glUniform1i(glGetUniformLocation(shader, "normalMap"), 1);

  glUniform1f(glGetUniformLocation(shader, "hitFlash"), 0.0f);

  // Bone transforms — computed once, shared by all instances
  auto transforms = model->getTransformsAtTime("sway", time);
  for (int i = 0; i < static_cast<int>(transforms.size()); ++i) {
    const std::string name = "finalBonesMatrices[" + std::to_string(i) + "]";
    glUniformMatrix4fv(glGetUniformLocation(shader, name.c_str()),
                       1, GL_FALSE, glm::value_ptr(transforms[i]));
  }

  constexpr float kGrassScale = 0.1f;
  const GLint modelLoc = glGetUniformLocation(shader, "model");

  for (const auto &pos : positions) {
    Matrix<4, 4> T  = translate<4, 4>(pos.x, pos.y, pos.z);
    Matrix<4, 4> Rx = rotateX<4, 4>(math_constants::kHalfPi);
    Matrix<4, 4> S  = ::scale<4, 4>(kGrassScale, kGrassScale, kGrassScale);
    auto glM = toOpenGLMatrix(T * Rx * S);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glM.data());
    model->draw(shader);
  }
}

}  // namespace

// =============================================================================
// SCENE LIFECYCLE
// =============================================================================

void MainMenu::loadScene() {
  using namespace render_constants;

  // Shaders
  groundShader_ = createShaderProgram("data/shaders/grass.vert",
                                      "data/shaders/grass.frag");
  animShader_   = createShaderProgram("data/shaders/anim_shader.vert",
                                      "data/shaders/anim_shader.frag");
  groundU_ = makeGroundUniforms(groundShader_);

  // Ground geometry
  groundMesh_ = createGrassMesh();

  // Ground PBR textures
  grassTex_.color        = loadTexture("data/textures/grass_color.png");
  grassTex_.normal       = loadTexture("data/textures/grass_normal.png");
  grassTex_.ao           = loadTexture("data/textures/grass_ambient_occlusion.png");
  grassTex_.roughness    = loadTexture("data/textures/grass_roughness.png");
  grassTex_.displacement = loadTexture("data/textures/grass_displacement.png");
  noiseTex_              = loadTexture("data/textures/perlin_noise.jpg", 3);

  // Grass model + animation (embedded in the same glb)
  grassColorTex_  = loadTexture("data/textures/grass_color.png");
  grassNormalTex_ = createDefaultNormalTexture();
  grassModel_     = std::make_unique<AnimatedModel>("data/models/world/grass.glb");
  grassModel_->loadAnimation("sway", "data/models/world/grass.glb");

  // Static texture-unit bindings (ground shader) — set once
  glUseProgram(groundShader_);
  glUniform1i(groundU_.grass, 0);
  glUniform1i(groundU_.noise, 1);
  glUniform1i(glGetUniformLocation(groundShader_, "normalMap"),      2);
  glUniform1i(glGetUniformLocation(groundShader_, "aoMap"),          3);
  glUniform1i(glGetUniformLocation(groundShader_, "roughnessMap"),   4);
  glUniform1i(glGetUniformLocation(groundShader_, "displacementMap"),5);
  glUniform3f(groundU_.fogColor, kFogColor.r, kFogColor.g, kFogColor.b);
  glUniform1f(groundU_.fogStart, kFogStart);
  glUniform1f(groundU_.fogEnd,   kFogEnd);

  // Moonlight (matches main game)
  moonLight_.direction = glm::normalize(glm::vec3(-0.4f,  1.2f, -0.5f));
  moonLight_.ambient   = glm::vec3(0.07f, 0.08f, 0.14f);
  moonLight_.diffuse   = glm::vec3(0.18f, 0.22f, 0.40f);
  moonLight_.specular  = glm::vec3(0.14f, 0.16f, 0.26f);

  // Deterministic grass instance positions (7×7 scattered grid)
  for (int i = -3; i <= 3; ++i) {
    for (int j = -3; j <= 3; ++j) {
      grassPositions_.push_back({
          i * 4.5f + std::sin(static_cast<float>(i) * 1.3f + j) * 1.5f,
          0.0f,
          j * 4.5f + std::cos(static_cast<float>(j) * 1.7f + i) * 1.5f
      });
    }
  }
}

void MainMenu::updateScene(float dt) {
  sceneTime_ += dt;
}

void MainMenu::renderScene(int fbW, int fbH) {
  using namespace render_constants;
  using namespace math_constants;

  // Camera — slow orbit around origin
  const float radius = 14.0f;
  const float angle  = sceneTime_ * 0.05f;
  const float cx     = std::cos(angle) * radius;
  const float cz     = std::sin(angle) * radius;
  const float cy     = 3.5f;

  const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
  cam_.setPerspective(kFovDegrees * kDegToRad, aspect, kNearPlane, kFarPlane);
  cam_.setLookAt(Vector<3>{cx, cy, cz}, Vector<3>{0.0f, 0.0f, 0.0f});

  const auto       glView  = toOpenGLMatrix(cam_.getViewMatrix());
  const auto       glProj  = toOpenGLMatrix(cam_.getProjectionMatrix());
  const glm::vec3  viewPos = {cx, cy, cz};

  const std::vector<PointLight> noLights;

  // -- Ground ----------------------------------------------------------------
  uploadCommonUniforms(groundShader_, moonLight_, noLights, viewPos, glView, glProj);
  renderGround(groundMesh_, grassTex_, noiseTex_);

  // -- Animated grass --------------------------------------------------------
  uploadCommonUniforms(animShader_, moonLight_, noLights, viewPos, glView, glProj);
  drawGrassInstances(animShader_, grassModel_.get(),
                     grassPositions_, grassColorTex_, grassNormalTex_, sceneTime_);
}

void MainMenu::unloadScene() {
  glDeleteProgram(groundShader_);
  glDeleteProgram(animShader_);
  deleteGpuMesh(groundMesh_);

  glDeleteTextures(1, &grassTex_.color);
  glDeleteTextures(1, &grassTex_.normal);
  glDeleteTextures(1, &grassTex_.ao);
  glDeleteTextures(1, &grassTex_.roughness);
  glDeleteTextures(1, &grassTex_.displacement);
  glDeleteTextures(1, &noiseTex_);
  glDeleteTextures(1, &grassColorTex_);
  glDeleteTextures(1, &grassNormalTex_);

  groundShader_ = 0;
  animShader_   = 0;
}

// =============================================================================
// UI OVERLAY
// =============================================================================

namespace {
const ImVec4 kMenuGold  = ImVec4(1.00f, 0.85f, 0.20f, 1.0f);
const ImVec4 kMenuMuted = ImVec4(0.65f, 0.60f, 0.50f, 0.85f);
}  // namespace

void MainMenu::renderUI(float W, float H, bool &outEnter, bool &outQuit) {
  // Dim vignette
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(W, H));
  ImGui::SetNextWindowBgAlpha(0.0f);
  ImGui::Begin("##mm_vignette", nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(0, 0), ImVec2(W, H), IM_COL32(4, 6, 15, 140));
  ImGui::End();

  // Center panel
  const float panelW = 520.0f;
  const float panelH = 350.0f;
  ImGui::SetNextWindowPos(ImVec2((W - panelW) * 0.5f, (H - panelH) * 0.5f));
  ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
  ImGui::SetNextWindowBgAlpha(0.92f);
  ImGui::Begin("##mm_panel", nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoNav);

  ImGui::Spacing();
  const char *title = "1348 d.C.: FERRO E SANGUE";
  ImGui::SetCursorPosX((panelW - ImGui::CalcTextSize(title).x) * 0.5f);
  ImGui::TextColored(kMenuGold, "%s", title);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  const char *sub = "Defenda o castelo. Sobreviva a noite.";
  ImGui::SetCursorPosX((panelW - ImGui::CalcTextSize(sub).x) * 0.5f);
  ImGui::TextColored(kMenuMuted, "%s", sub);

  ImGui::Spacing();
  ImGui::Spacing();
  ImGui::Spacing();
  ImGui::Spacing();

  const float btnW = 230.0f;
  const float btnH =  50.0f;
  const float btnX = (panelW - btnW) * 0.5f;

  ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.30f, 0.18f, 0.04f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.55f, 0.32f, 0.06f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.70f, 0.42f, 0.08f, 1.0f));

  ImGui::SetCursorPosX(btnX);
  if (ImGui::Button("  JOGAR  ", ImVec2(btnW, btnH))) outEnter = true;
  ImGui::Spacing();
  ImGui::SetCursorPosX(btnX);
  if (ImGui::Button("  SAIR  ",  ImVec2(btnW, btnH))) outQuit  = true;

  ImGui::PopStyleColor(3);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  const char *hint = "WASD: mover   |   Mouse: olhar   |   T: posicionar tropas";
  ImGui::SetCursorPosX((panelW - ImGui::CalcTextSize(hint).x) * 0.5f);
  ImGui::TextColored(kMenuMuted, "%s", hint);

  ImGui::End();
}

// =============================================================================
// RUN LOOP
// =============================================================================

bool MainMenu::run(GLFWwindow *window) {
  loadScene();

  double prevTime = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    const double now = glfwGetTime();
    const float  dt  = static_cast<float>(now - prevTime);
    prevTime         = now;

    glfwPollEvents();

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);

    updateScene(dt);

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, fbW, fbH);
    glClearColor(render_constants::kFogColor.r,
                 render_constants::kFogColor.g,
                 render_constants::kFogColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderScene(fbW, fbH);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    bool enterClicked = false, quitClicked = false;
    renderUI(static_cast<float>(fbW), static_cast<float>(fbH),
             enterClicked, quitClicked);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);

    if (enterClicked) { unloadScene(); return true;  }
    if (quitClicked)  { unloadScene(); return false; }
  }

  unloadScene();
  return false;
}
