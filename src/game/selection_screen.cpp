#include "game/selection_screen.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "engine/obj_loader.h"
#include "engine/shader_program.h"
#include "engine/texture_loader.h"
#include "math/constants.h"
#include "math/matrix_ops.h"
#include "math/opengl_utils.h"
#include "math/transforms.h"
#include "render/render_constants.h"
#include "render/scene_renderer.h"   // uploadCommonUniforms

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

namespace {

// Draw a static mesh. rxRad optionally rotates around X (e.g. -pi/2 to lay flat).
void drawStatic(GLuint shader, GLint modelLoc,
                Mesh &mesh, GLuint tex,
                float tx, float ty, float tz,
                float sx, float sy, float sz,
                float rxRad = 0.0f) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(glGetUniformLocation(shader, "tex"), 0);

  Matrix<4, 4> T  = translate<4, 4>(tx, ty, tz);
  Matrix<4, 4> Rx = rotateX<4, 4>(rxRad);
  Matrix<4, 4> S  = ::scale<4, 4>(sx, sy, sz);
  auto glM = toOpenGLMatrix(T * Rx * S);
  glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glM.data());

  mesh.draw();
}

// Draw a general (AnimatedModel) with the given animation at the given time.
void drawGeneral(GLuint shader,
                 AnimatedModel &model,
                 const std::string &animName,
                 float  animTime,
                 const glm::vec3 &pos,
                 GLuint colorTex,
                 GLuint defaultNormal) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, colorTex);
  glUniform1i(glGetUniformLocation(shader, "tex"), 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, defaultNormal);
  glUniform1i(glGetUniformLocation(shader, "normalMap"), 1);

  glUniform1f(glGetUniformLocation(shader, "hitFlash"), 0.0f);

  // Bone transforms
  auto transforms = model.getTransformsAtTime(animName, animTime);
  for (int i = 0; i < static_cast<int>(transforms.size()); ++i) {
    const std::string name = "finalBonesMatrices[" + std::to_string(i) + "]";
    glUniformMatrix4fv(glGetUniformLocation(shader, name.c_str()),
                       1, GL_FALSE, glm::value_ptr(transforms[i]));
  }

  // Model matrix — same convention as GameObject::draw()
  // Characters in this project are exported at ~100× world scale, so 0.01 normalises them.
  constexpr float kGeneralScale = 0.01f;
  Matrix<4, 4> T  = translate<4, 4>(pos.x, pos.y, pos.z);
  Matrix<4, 4> Ry = rotateY<4, 4>(0.0f);
  Matrix<4, 4> Rx = rotateX<4, 4>(math_constants::kHalfPi);
  Matrix<4, 4> S  = ::scale<4, 4>(kGeneralScale, kGeneralScale, kGeneralScale);
  auto glM = toOpenGLMatrix(T * Ry * Rx * S);
  glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glM.data());

  model.draw(shader);
}

// ---------------------------------------------------------------------------
// Weapon matrix helpers — mirror the ones in scene_renderer.cpp
// ---------------------------------------------------------------------------
static glm::mat4 wTrans(float tx, float ty, float tz) {
  glm::mat4 m(0.0f);
  m[0][0]=1; m[1][1]=1; m[2][2]=1; m[3][3]=1;
  m[3][0]=tx; m[3][1]=ty; m[3][2]=tz;
  return m;
}
static glm::mat4 wRotX(float r) {
  float c=std::cos(r), s=std::sin(r);
  glm::mat4 m(0.0f);
  m[0][0]=1; m[1][1]=c; m[1][2]=s; m[2][1]=-s; m[2][2]=c; m[3][3]=1;
  return m;
}
static glm::mat4 wRotY(float r) {
  float c=std::cos(r), s=std::sin(r);
  glm::mat4 m(0.0f);
  m[0][0]=c; m[0][2]=-s; m[1][1]=1; m[2][0]=s; m[2][2]=c; m[3][3]=1;
  return m;
}
static glm::mat4 wRotZ(float r) {
  float c=std::cos(r), s=std::sin(r);
  glm::mat4 m(0.0f);
  m[0][0]=c; m[0][1]=s; m[1][0]=-s; m[1][1]=c; m[2][2]=1; m[3][3]=1;
  return m;
}
static glm::mat4 wScale(float s) {
  glm::mat4 m(0.0f);
  m[0][0]=s; m[1][1]=s; m[2][2]=s; m[3][3]=1;
  return m;
}

// Bow offset — matches renderDefenderWeapons (archer left hand)
static glm::mat4 kBowOffset() {
  glm::mat4 m(0.0f);
  constexpr float s = 5.0f;
  m[0][2] = s; m[1][1] = s; m[2][0] = -s; m[3][3] = 1.0f; m[3][1] = -7.0f;
  return m;
}

// Sword offset — based on KnightWeaponTweaks defaults (knight right hand)
static glm::mat4 kSwordOffset() {
  return wTrans(31.25f, -6.25f, -37.5f)
       * wRotX(-0.196f) * wRotY(-0.589f) * wRotZ(1.374f)
       * wScale(128.031f);
}

// Draw a weapon mesh attached to a named bone of an AnimatedModel.
// getTransformsAtTime() must have been called on 'model' before this.
void drawGeneralWeapon(GLuint objShader, GLint modelLoc,
                       AnimatedModel &model,
                       const glm::vec3 &pos,
                       Mesh &weaponMesh, GLuint weaponTex,
                       const std::string &boneName,
                       const glm::mat4 &weaponOffset) {
  // General model-to-world matrix (same transform as drawGeneral)
  constexpr float kS = 0.01f;
  Matrix<4,4> T  = translate<4,4>(pos.x, pos.y, pos.z);
  Matrix<4,4> Rx = rotateX<4,4>(math_constants::kHalfPi);
  Matrix<4,4> S  = ::scale<4,4>(kS, kS, kS);
  auto arr = toOpenGLMatrix(T * Rx * S);
  glm::mat4 modelMat = glm::make_mat4(arr.data());

  glm::mat4 boneWorld = model.getNodeGlobalTransform(boneName);
  glm::mat4 finalMat  = modelMat * boneWorld * weaponOffset;

  glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(finalMat));
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, weaponTex);
  glUniform1i(glGetUniformLocation(objShader, "tex"), 0);
  weaponMesh.draw();
}

}  // namespace

// =============================================================================
// SCENE LIFECYCLE
// =============================================================================

void SelectionScreen::loadScene() {
  using namespace render_constants;

  // --- Shaders ---------------------------------------------------------------
  objShader_  = createShaderProgram("data/shaders/shader.vert",
                                    "data/shaders/shader.frag");
  animShader_ = createShaderProgram("data/shaders/anim_shader.vert",
                                    "data/shaders/anim_shader.frag");
  objU_ = makeObjUniforms(objShader_);

  // shader.frag has fog uniforms that default to 0/0 → fogFactor = 1 → pitch black.
  // Push fog far away so nothing is fogged in the selection scene.
  glUseProgram(objShader_);
  glUniform3f(glGetUniformLocation(objShader_, "fogColor"),
              kFogColor.r, kFogColor.g, kFogColor.b);
  glUniform1f(glGetUniformLocation(objShader_, "fogStart"), 200.0f);
  glUniform1f(glGetUniformLocation(objShader_, "fogEnd"),   400.0f);
  glUniform1i(glGetUniformLocation(objShader_, "numPointLights"), 0);

  // --- Textures --------------------------------------------------------------
  floorTex_         = loadTexture("data/textures/floor.png");
  wallTex_          = loadTexture("data/textures/wall.png");
  defaultNormalTex_ = createDefaultNormalTexture();

  generalTex_[0] = loadTexture("data/textures/english.png");
  generalTex_[1] = loadTexture("data/textures/byzantine.png");
  generalTex_[2] = loadTexture("data/textures/turk.png");

  // --- Weapons ---------------------------------------------------------------
  bowTex_   = loadTexture("data/textures/bow.jpg");
  swordTex_ = loadTexture("data/textures/knightattachment.png");

  std::vector<Vertex> bowVerts;
  if (loadObj("data/models/archer/bow.obj", bowVerts) && !bowVerts.empty())
    bowMesh_ = std::make_unique<Mesh>(bowVerts);

  std::vector<Vertex> swordVerts;
  if (loadObj("data/models/knight/sword.obj", swordVerts) && !swordVerts.empty())
    swordMesh_ = std::make_unique<Mesh>(swordVerts);

  // --- Banners (one model + one texture per faction) -------------------------
  bannerTex_[0] = loadTexture("data/textures/faction_banner_england.png");
  bannerTex_[1] = loadTexture("data/textures/faction_banner_byzantium.png");
  bannerTex_[2] = loadTexture("data/textures/faction_banner_turks.png");

  const char *kBannerObjs[3] = {
    "data/models/world/english_banner.obj",
    "data/models/world/byzantium_banner.obj",
    "data/models/world/turk_banner.obj",
  };
  for (int i = 0; i < 3; ++i) {
    std::vector<Vertex> bv;
    if (loadObj(kBannerObjs[i], bv) && !bv.empty())
      bannerMesh_[i] = std::make_unique<Mesh>(bv);
  }

  // --- Wall (loaded from OBJ, used for both back wall and floor) ------------
  std::vector<Vertex> wallVerts;
  if (loadObj("data/models/world/wall.obj", wallVerts) && !wallVerts.empty()) {
    wallMesh_ = std::make_unique<Mesh>(wallVerts);
  }

  // --- Generals --------------------------------------------------------------
  // English
  generals_[0] = std::make_unique<AnimatedModel>("data/models/generals/englishT.glb");
  generals_[0]->loadAnimation("idle",     "data/models/generals/english_idle.glb");
  generals_[0]->loadAnimation("selected", "data/models/generals/englishselected.glb");

  // Byzantine
  generals_[1] = std::make_unique<AnimatedModel>("data/models/generals/byzantineT.glb");
  generals_[1]->loadAnimation("idle",     "data/models/generals/byzantine_idle.glb");
  generals_[1]->loadAnimation("selected", "data/models/generals/byzantineselected.glb");

  // Turk
  generals_[2] = std::make_unique<AnimatedModel>("data/models/generals/turkT.glb");
  generals_[2]->loadAnimation("idle",     "data/models/generals/turk_idle.glb");
  generals_[2]->loadAnimation("selected", "data/models/generals/turkselected.glb");

  // General world positions
  generalPos_[0] = {-2.2f, 0.0f, 0.0f};
  generalPos_[1] = { 0.0f, 0.0f, 0.0f};
  generalPos_[2] = { 2.2f, 0.0f, 0.0f};

  // --- Ember particles -------------------------------------------------------
  particleShader_ = createShaderProgram("data/shaders/particle.vert",
                                        "data/shaders/particle.frag");
  emberTex_ = loadTexture("data/textures/ember.png", 4);

  // Billboard quad (same layout as CannonSmoke)
  static const float kEmberQuad[] = {
    -0.5f,-0.5f,0,  0,0,
     0.5f,-0.5f,0,  1,0,
     0.5f, 0.5f,0,  1,1,
    -0.5f,-0.5f,0,  0,0,
     0.5f, 0.5f,0,  1,1,
    -0.5f, 0.5f,0,  0,1,
  };
  glGenVertexArrays(1, &emberVao_);
  glGenBuffers(1, &emberVbo_);
  glBindVertexArray(emberVao_);
  glBindBuffer(GL_ARRAY_BUFFER, emberVbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kEmberQuad), kEmberQuad, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
  glBindVertexArray(0);

  // --- Free cam initial state ------------------------------------------------
  camPos_   = {0.0f, 2.0f, 6.5f};
  camYaw_   = -1.5708f;   // -pi/2 → forward=(0,0,-1)
  camPitch_ = -0.15f;

  // --- Lighting --------------------------------------------------------------
  // Direction must have positive Z so the light faces the generals (+Z normals).
  moonLight_.direction = glm::normalize(glm::vec3(0.2f, 0.9f, 0.6f));
  moonLight_.ambient   = glm::vec3(0.55f, 0.52f, 0.48f);
  moonLight_.diffuse   = glm::vec3(0.85f, 0.82f, 0.75f);
  moonLight_.specular  = glm::vec3(0.40f, 0.38f, 0.32f);
}

void SelectionScreen::updateScene(float dt) {
  sceneTime_ += dt;

  for (int i = 0; i < 3; ++i)
    generalAnimTime_[i] += dt;

  // --- Ember spawn -----------------------------------------------------------
  emberSpawnAccum_ += dt;
  constexpr float kSpawnInterval = 0.35f;  // ~3 embers/sec
  while (emberSpawnAccum_ >= kSpawnInterval) {
    emberSpawnAccum_ -= kSpawnInterval;
    EmberParticle p;
    p.position = {
        -5.0f + (std::rand() % 100) * 0.01f,           // x: -5 .. -4 (left)
         0.2f + (std::rand() % 100) * 0.022f,           // y: 0.2 .. 2.4
         1.0f + (std::rand() % 100) * 0.02f             // z: 1..3, in front of generals
    };
    p.velocity = {
        0.25f + (std::rand() % 100) * 0.003f,           // +X drift (left→right)
        (std::rand() % 100 - 50) * 0.0008f,             // tiny Y wobble
        0.0f
    };
    p.lifetime = 15.0f + (std::rand() % 100) * 0.05f;
    p.size     = 0.10f + (std::rand() % 100) * 0.002f;  // bigger for visibility
    p.age      = 0.0f;
    embers_.push_back(p);
  }

  // --- Ember update ----------------------------------------------------------
  for (auto &e : embers_) {
    e.age      += dt;
    e.position += e.velocity * dt;
  }
  embers_.erase(
      std::remove_if(embers_.begin(), embers_.end(),
                     [](const EmberParticle &p){ return p.age >= p.lifetime; }),
      embers_.end());
}

void SelectionScreen::renderScene(int fbW, int fbH) {
  using namespace render_constants;
  using namespace math_constants;

  // Free camera
  const glm::vec3 fwd = {
      std::cos(camYaw_) * std::cos(camPitch_),
      std::sin(camPitch_),
      std::sin(camYaw_) * std::cos(camPitch_)
  };
  const glm::vec3 lookAt = camPos_ + fwd;

  const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
  cam_.setPerspective(kFovDegrees * kDegToRad, aspect, kNearPlane, kFarPlane);
  cam_.setLookAt(Vector<3>{camPos_.x, camPos_.y, camPos_.z},
                 Vector<3>{lookAt.x,  lookAt.y,  lookAt.z});

  const auto      glView  = toOpenGLMatrix(cam_.getViewMatrix());
  const auto      glProj  = toOpenGLMatrix(cam_.getProjectionMatrix());
  const glm::vec3 viewPos = camPos_;

  const std::vector<PointLight> noLights;

  // --- Environment (both use wallMesh_, different textures/transforms) -------
  uploadCommonUniforms(objShader_, moonLight_, noLights, viewPos, glView, glProj);
  if (wallMesh_) {
    // Floor: rotated flat (-90° X).
    // sy controls depth (wall height → floor depth after rotation),
    // sz controls thickness → squish to ~0.
    drawStatic(objShader_, objU_.model, *wallMesh_, floorTex_,
               0.0f, 0.0f, 0.0f,
               floorSX_, floorSY_, 0.01f,
               -math_constants::kHalfPi);

    // Back wall: standing upright.
    drawStatic(objShader_, objU_.model, *wallMesh_, wallTex_,
               0.0f, 0.0f, wallZ_,
               wallSX_, wallSY_, 1.0f);
  }

  // --- Banners (one per general, behind them) --------------------------------
  for (int i = 0; i < 3; ++i) {
    if (!bannerMesh_[i]) continue;
    drawStatic(objShader_, objU_.model, *bannerMesh_[i], bannerTex_[i],
               generalPos_[i].x, 0.0f, generalPos_[i].z + bannerZOffset_,
               bannerScale_, bannerScale_, bannerScale_);
  }

  // --- Generals --------------------------------------------------------------
  uploadCommonUniforms(animShader_, moonLight_, noLights, viewPos, glView, glProj);

  for (int i = 0; i < 3; ++i) {
    if (!generals_[i]) continue;
    const std::string &anim = (i == selectedGeneral_) ? "selected" : "idle";
    drawGeneral(animShader_, *generals_[i], anim, generalAnimTime_[i],
                generalPos_[i], generalTex_[i], defaultNormalTex_);
  }

  // --- Weapons (attached to bone, rendered with objShader) ------------------
  uploadCommonUniforms(objShader_, moonLight_, noLights, viewPos, glView, glProj);

  // English (index 0) → bow, left hand
  if (generals_[0] && bowMesh_ && bowTex_)
    drawGeneralWeapon(objShader_, objU_.model, *generals_[0], generalPos_[0],
                      *bowMesh_, bowTex_,
                      "mixamorig:LeftHand", kBowOffset());

  // Byzantine (index 1) → sword, right hand
  if (generals_[1] && swordMesh_ && swordTex_)
    drawGeneralWeapon(objShader_, objU_.model, *generals_[1], generalPos_[1],
                      *swordMesh_, swordTex_,
                      "mixamorig:RightHand", kSwordOffset());

  // Turk (index 2) → sword, right hand
  if (generals_[2] && swordMesh_ && swordTex_)
    drawGeneralWeapon(objShader_, objU_.model, *generals_[2], generalPos_[2],
                      *swordMesh_, swordTex_,
                      "mixamorig:RightHand", kSwordOffset());

  // --- Ember particles -------------------------------------------------------
  if (!embers_.empty()) {
    // Camera right/up from free-cam vectors
    const glm::vec3 fwdN  = glm::normalize(lookAt - camPos_);
    const glm::vec3 right = glm::normalize(glm::cross(fwdN, glm::vec3(0, 1, 0)));
    const glm::vec3 up    = glm::normalize(glm::cross(right, fwdN));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glUseProgram(particleShader_);
    glUniformMatrix4fv(glGetUniformLocation(particleShader_, "view"),       1, GL_FALSE, glView.data());
    glUniformMatrix4fv(glGetUniformLocation(particleShader_, "projection"), 1, GL_FALSE, glProj.data());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, emberTex_);
    glUniform1i(glGetUniformLocation(particleShader_, "tex"),   0);
    glUniform1f(glGetUniformLocation(particleShader_, "alpha"), 1.0f);  // no fade

    const GLint modelLoc = glGetUniformLocation(particleShader_, "model");
    glBindVertexArray(emberVao_);

    for (const auto &e : embers_) {
      glm::mat4 model(0.0f);
      model[0] = glm::vec4(right * e.size, 0.0f);
      model[1] = glm::vec4(up    * e.size, 0.0f);
      model[2] = glm::vec4(glm::cross(right, up) * e.size, 0.0f);
      model[3] = glm::vec4(e.position, 1.0f);
      glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
      glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
  }
}

void SelectionScreen::unloadScene() {
  // GL resources not freed by Mesh destructor
  if (wallMesh_) {
    glDeleteVertexArrays(1, &wallMesh_->vao);
    glDeleteBuffers(1, &wallMesh_->vbo);
  }

  glDeleteProgram(objShader_);
  glDeleteProgram(animShader_);
  glDeleteProgram(particleShader_);
  if (bowMesh_)   { glDeleteVertexArrays(1,&bowMesh_->vao);   glDeleteBuffers(1,&bowMesh_->vbo); }
  if (swordMesh_) { glDeleteVertexArrays(1,&swordMesh_->vao); glDeleteBuffers(1,&swordMesh_->vbo); }
  for (int i = 0; i < 3; ++i) {
    if (bannerMesh_[i]) {
      glDeleteVertexArrays(1, &bannerMesh_[i]->vao);
      glDeleteBuffers(1, &bannerMesh_[i]->vbo);
    }
  }
  glDeleteTextures(3, bannerTex_);
  glDeleteTextures(1, &floorTex_);
  glDeleteTextures(1, &wallTex_);
  glDeleteTextures(1, &defaultNormalTex_);
  glDeleteTextures(1, &bowTex_);
  glDeleteTextures(1, &swordTex_);
  glDeleteTextures(1, &emberTex_);
  glDeleteVertexArrays(1, &emberVao_);
  glDeleteBuffers(1, &emberVbo_);
  glDeleteTextures(3, generalTex_);

  objShader_  = 0;
  animShader_ = 0;
}

// =============================================================================
// UI OVERLAY
// =============================================================================

namespace {
const ImVec4 kSelGold    = ImVec4(1.00f, 0.85f, 0.20f, 1.0f);
const ImVec4 kSelCream   = ImVec4(0.92f, 0.88f, 0.78f, 1.0f);
const ImVec4 kSelMuted   = ImVec4(0.65f, 0.60f, 0.50f, 0.80f);
const ImVec4 kSelDimmed  = ImVec4(0.40f, 0.36f, 0.28f, 1.0f);

const char *kGeneralNames[3] = {"Ingleses", "Bizantinos", "Turcos"};

struct FactionInfo {
  const char *arma;
  const char *tropa;
  const char *especialidade;
};

const FactionInfo kFactionInfo[3] = {
  { "Arco e Flecha",  "Arqueiros",         "Ataques a distancia com alto alcance" },
  { "Espada",         "Cavalaria Pesada",  "Alta resistencia e dano corpo a corpo" },
  { "Espada",         "Guerreiros Ageis",  "Versateis e rapidos no campo de batalha" },
};
}  // namespace

void SelectionScreen::renderUI(float W, float H, bool &outBegin, bool &outBack) {
  // --- Bottom-center selection strip
  const float panelW = 560.0f;
  const float panelH = 230.0f;
  ImGui::SetNextWindowPos(ImVec2((W - panelW) * 0.5f, H - panelH - 30.0f));
  ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
  ImGui::SetNextWindowBgAlpha(0.90f);
  ImGui::Begin("##sel_panel", nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoNav);

  // Title row
  ImGui::Spacing();
  const char *title = "ESCOLHA SEU GENERAL";
  ImGui::SetCursorPosX((panelW - ImGui::CalcTextSize(title).x) * 0.5f);
  ImGui::TextColored(kSelGold, "%s", title);
  ImGui::Separator();
  ImGui::Spacing();

  // General name selectors  ←  [Name]  →
  const float arrowW = 30.0f;
  const float nameW  = 160.0f;
  const float rowW   = arrowW + nameW + arrowW + 8.0f;
  ImGui::SetCursorPosX((panelW - rowW) * 0.5f);

  ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.12f, 0.03f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.45f, 0.26f, 0.06f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.60f, 0.36f, 0.08f, 1.0f));

  if (ImGui::Button(" < ##left", ImVec2(arrowW, 0))) {
    selectedGeneral_                    = (selectedGeneral_ + 2) % 3;
    generalAnimTime_[selectedGeneral_]  = 0.0f;
    leftWasDown_                        = true;
  }
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::Button(kGeneralNames[selectedGeneral_], ImVec2(nameW, 0));
  ImGui::PopStyleColor(3);
  ImGui::SameLine(0.0f, 4.0f);
  if (ImGui::Button(" > ##right", ImVec2(arrowW, 0))) {
    selectedGeneral_                    = (selectedGeneral_ + 1) % 3;
    generalAnimTime_[selectedGeneral_]  = 0.0f;
    rightWasDown_                       = true;
  }

  ImGui::PopStyleColor(3);

  // Three names with the active one highlighted
  ImGui::Spacing();
  const float nameColW = panelW / 3.0f;
  for (int i = 0; i < 3; ++i) {
    const ImVec4 &col = (i == selectedGeneral_) ? kSelGold : kSelDimmed;
    const float   tw  = ImGui::CalcTextSize(kGeneralNames[i]).x;
    ImGui::SetCursorPosX(nameColW * i + (nameColW - tw) * 0.5f);
    if (i > 0) ImGui::SameLine(nameColW * i + (nameColW - tw) * 0.5f);
    ImGui::TextColored(col, "%s", kGeneralNames[i]);
  }

  // Faction details
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  const FactionInfo &fi = kFactionInfo[selectedGeneral_];
  const float labelX = (panelW - 420.0f) * 0.5f;
  ImGui::SetCursorPosX(labelX);
  ImGui::TextColored(kSelMuted,  "Arma:");
  ImGui::SameLine();
  ImGui::TextColored(kSelCream, " %s", fi.arma);

  ImGui::SetCursorPosX(labelX);
  ImGui::TextColored(kSelMuted, "Tropa:");
  ImGui::SameLine();
  ImGui::TextColored(kSelCream, " %s", fi.tropa);

  ImGui::SetCursorPosX(labelX);
  ImGui::TextColored(kSelMuted, "Especialidade:");
  ImGui::SameLine();
  ImGui::TextColored(kSelCream, " %s", fi.especialidade);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // VOLTAR / INICIAR buttons
  const float btnW     = 150.0f;
  const float btnH     =  36.0f;
  const float gap      =  16.0f;
  const float totalW   = btnW * 2.0f + gap;
  ImGui::SetCursorPosX((panelW - totalW) * 0.5f);

  ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.10f, 0.05f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.30f, 0.20f, 0.07f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.42f, 0.28f, 0.08f, 1.0f));
  if (ImGui::Button("  VOLTAR  ", ImVec2(btnW, btnH))) outBack = true;
  ImGui::PopStyleColor(3);

  ImGui::SameLine(0.0f, gap);

  ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.30f, 0.18f, 0.04f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.55f, 0.32f, 0.06f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.70f, 0.42f, 0.08f, 1.0f));
  if (ImGui::Button("  INICIAR  ", ImVec2(btnW, btnH))) outBegin = true;
  ImGui::PopStyleColor(3);

  ImGui::End();
}

// =============================================================================
// RUN LOOP
// =============================================================================

bool SelectionScreen::run(GLFWwindow *window) {
  loadScene();

  double prevTime = glfwGetTime();
  bool   result   = false;
  bool   exitLoop = false;

  while (!glfwWindowShouldClose(window) && !exitLoop) {
    const double now = glfwGetTime();
    const float  dt  = static_cast<float>(now - prevTime);
    prevTime         = now;

    glfwPollEvents();

    // General selection (arrow keys, edge-triggered)
    const bool leftNow  = glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS;
    const bool rightNow = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    if (leftNow && !leftWasDown_) {
      selectedGeneral_                   = (selectedGeneral_ + 2) % 3;
      generalAnimTime_[selectedGeneral_] = 0.0f;
    }
    if (rightNow && !rightWasDown_) {
      selectedGeneral_                   = (selectedGeneral_ + 1) % 3;
      generalAnimTime_[selectedGeneral_] = 0.0f;
    }
    leftWasDown_  = leftNow;
    rightWasDown_ = rightNow;

    // Render
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

    bool beginClicked = false, backClicked = false;
    renderUI(static_cast<float>(fbW), static_cast<float>(fbH),
             beginClicked, backClicked);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);

    if (beginClicked) { result = true;  exitLoop = true; }
    if (backClicked)  { result = false; exitLoop = true; }
  }

  unloadScene();
  return result;
}
