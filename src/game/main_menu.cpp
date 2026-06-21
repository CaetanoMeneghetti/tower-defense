#include "game/main_menu.h"

#include <cmath>
#include <cstdlib>
#include <string>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "engine/audio.h"
#include "engine/catmull_rom.h"
#include "engine/obj_loader.h"
#include "engine/shader_program.h"
#include "engine/texture_loader.h"
#include "math/constants.h"
#include "world/path_generator.h"
#include "math/matrix_ops.h"
#include "math/opengl_utils.h"
#include "math/transforms.h"
#include "render/render_constants.h"

// =============================================================================
// LOCAL CONSTANTS + HELPERS
// =============================================================================

namespace {

struct CinematicShot {
  glm::vec3 camStart, camEnd;
  glm::vec3 lookStart, lookEnd;
  float fovDeg;
  float duration;
};

// Archers/arquebus/knight at Z ≈ -5 facing +Z.
// Knights are slightly forward (Z = -2, melee range).
// Zombies walk from +Z toward -Z.
static const CinematicShot kShots[3] = {
    // Shot 0: side angle, archers in foreground
    {
        glm::vec3(12, 3.5f, -2), glm::vec3(7, 3.0f, -2),
        glm::vec3(-1, 1.2f, -4), glm::vec3(-1, 1.0f, -4),
        58.0f, 9.0f
    },
    // Shot 1: behind zombie horde looking at the defensive line
    {
        glm::vec3(0.5f, 3.5f, 18), glm::vec3(0.5f, 3.0f, 14),
        glm::vec3(0,    0.9f, -5), glm::vec3(0,    0.7f, -5),
        52.0f, 9.0f
    },
    // Shot 2: over an archer's shoulder, telephoto
    {
        glm::vec3(-3.5f, 2.5f, -7), glm::vec3(-3.5f, 2.2f, -7),
        glm::vec3(  0.5f, 1.6f, 10), glm::vec3(  0.5f, 1.3f, 10),
        40.0f, 9.0f
    },
};

constexpr float kFadeDuration    = 1.0f;
constexpr float kZombieSpeed     = 1.9f;
constexpr float kZombieDeathZ    = -4.5f;  // fallback if no knight catches them
constexpr float kKnightRange     = 2.2f;   // melee range from knight Z
constexpr float kDeathDuration   = 3.0f;
constexpr float kAimDuration     = 2.0f;
constexpr float kFireDuration    = 1.8f;
constexpr float kAttackDuration  = 1.4f;
constexpr float kActorScale      = 0.01f;

// Bow offset — mirrors makeBowOffset() from main.cpp
glm::mat4 makeBowOffset() {
  glm::mat4 o = glm::mat4(0.0f);
  const float s = 5.0f;
  o[0][2] =  s;
  o[1][1] =  s;
  o[2][0] = -s;
  o[3][3] =  1.0f;
  o[3][1] = -7.0f;
  return o;
}

// Arquebus weapon offset — mirrors makeArquebusOffset()
glm::mat4 makeArqOffset() {
  const float s = 8.0f;
  Matrix<4, 4> mScale = ::scale<4, 4>(s, s, s);
  Matrix<4, 4> mRotY  = rotateY<4, 4>(-math_constants::kHalfPi);
  Matrix<4, 4> mRotX  = rotateX<4, 4>(-20.0f * math_constants::kDegToRad);
  Matrix<4, 4> mRotZ  = rotateZ<4, 4>(-math_constants::kHalfPi);
  Matrix<4, 4> mTrans = translate<4, 4>(0.0f, 0.0f, -4.0f);
  Matrix<4, 4> m = mTrans * mRotY * mRotX * mScale * mRotZ;
  return glm::make_mat4(toOpenGLMatrix(m).data());
}

// Knight sword offset — matches KnightWeaponTweaks defaults
glm::mat4 makeSwordOffset() {
  Matrix<4, 4> m = translate<4, 4>(31.25f, -6.25f, -37.5f) *
                   rotateX<4, 4>(-0.196f) *
                   rotateY<4, 4>(-0.589f) *
                   rotateZ<4, 4>(1.374f) *
                   scale<4, 4>(128.031f, 128.031f, 128.031f);
  return glm::make_mat4(toOpenGLMatrix(m).data());
}

glm::mat4 makeShieldOffset() {
  Matrix<4, 4> m = translate<4, 4>(3.75f, -43.75f, -8.75f) *
                   rotateY<4, 4>(2.454f) *
                   scale<4, 4>(180.0f, 180.0f, 180.0f);
  return glm::make_mat4(toOpenGLMatrix(m).data());
}

// Animated grass instances helper (same as before)
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

  auto transforms = model->getTransformsAtTime("sway", time);
  for (int i = 0; i < (int)transforms.size(); ++i) {
    std::string name = "finalBonesMatrices[" + std::to_string(i) + "]";
    glUniformMatrix4fv(glGetUniformLocation(shader, name.c_str()),
                       1, GL_FALSE, glm::value_ptr(transforms[i]));
  }

  const GLint modelLoc = glGetUniformLocation(shader, "model");
  constexpr float kGrassScale = 0.1f;
  for (const auto &pos : positions) {
    Matrix<4,4> T  = translate<4,4>(pos.x, pos.y, pos.z);
    Matrix<4,4> Rx = rotateX<4,4>(math_constants::kHalfPi);
    Matrix<4,4> S  = ::scale<4,4>(kGrassScale, kGrassScale, kGrassScale);
    auto glM = toOpenGLMatrix(T * Rx * S);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glM.data());
    model->draw(shader);
  }
}

// Draw an animated body via a proxy GameObject, then return whether to draw weapon.
// Caller must update proxy->position, currentAnimation, animationTime, loopAnim, rotationY
// before calling this.
void drawBody(GLuint animShader, GameObject &proxy,
              GLuint colorTex, GLuint normalTex) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, colorTex);
  glUniform1i(cachedUniformLocation(animShader, "tex"), 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, normalTex);
  glUniform1i(cachedUniformLocation(animShader, "normalMap"), 1);
  glUniform1f(cachedUniformLocation(animShader, "hitFlash"), 0.0f);
  proxy.draw(animShader);
}

void drawWeapon(GLuint objShader, const ObjUniforms &u,
                GameObject &proxy, const char *boneName,
                Mesh &mesh, GLuint tex, const glm::mat4 &offset) {
  glm::mat4 bone = proxy.getBoneWorldTransform(boneName);
  glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(bone * offset));
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(cachedUniformLocation(objShader, "tex"), 0);
  mesh.draw();
}

// Load an OBJ file, return Mesh or nullptr on failure.
std::unique_ptr<Mesh> loadMesh(const char *path) {
  std::vector<Vertex> verts;
  if (!loadObj(path, verts) || verts.empty()) return nullptr;
  return std::make_unique<Mesh>(verts);
}

static const float kQuadVerts[] = {
    -1.0f,-1.0f, 0.0f,0.0f,
     1.0f,-1.0f, 1.0f,0.0f,
     1.0f, 1.0f, 1.0f,1.0f,
    -1.0f,-1.0f, 0.0f,0.0f,
     1.0f, 1.0f, 1.0f,1.0f,
    -1.0f, 1.0f, 0.0f,1.0f,
};

}  // namespace

// =============================================================================
// FBO
// =============================================================================

void MainMenu::ensureFbo(int w, int h) {
  if (fboW_ == w && fboH_ == h && fbo_) return;
  if (fbo_) {
    glDeleteFramebuffers(1, &fbo_);
    glDeleteTextures(1, &fboColorTex_);
    glDeleteRenderbuffers(1, &fboDepthRbo_);
  }
  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glGenTextures(1, &fboColorTex_);
  glBindTexture(GL_TEXTURE_2D, fboColorTex_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboColorTex_, 0);
  glGenRenderbuffers(1, &fboDepthRbo_);
  glBindRenderbuffer(GL_RENDERBUFFER, fboDepthRbo_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, fboDepthRbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  fboW_ = w; fboH_ = h;
}

// =============================================================================
// LOAD
// =============================================================================

void MainMenu::loadScene() {
  using namespace render_constants;

  // ---- shaders ----
  groundShader_ = createShaderProgram("data/shaders/grass.vert", "data/shaders/grass.frag");
  groundU_      = makeGroundUniforms(groundShader_);
  animShader_   = createShaderProgram("data/shaders/anim_shader.vert", "data/shaders/anim_shader.frag");
  objShader_    = createShaderProgram("data/shaders/shader.vert",  "data/shaders/shader.frag");
  treeShader_   = createShaderProgram("data/shaders/tree_instanced.vert", "data/shaders/shader.frag");
  postShader_   = createShaderProgram("data/shaders/cinematic_post.vert", "data/shaders/cinematic_post.frag");
  objU_ = makeObjUniforms(objShader_);

  // ---- ground (grama estática) ----
  glUseProgram(groundShader_);
  glUniform1i(groundU_.grass, 0);
  glUniform1i(groundU_.noise, 1);
  glUniform1i(glGetUniformLocation(groundShader_, "normalMap"),       2);
  glUniform1i(glGetUniformLocation(groundShader_, "aoMap"),           3);
  glUniform1i(glGetUniformLocation(groundShader_, "roughnessMap"),    4);
  glUniform1i(glGetUniformLocation(groundShader_, "displacementMap"), 5);
  glUniform3f(groundU_.fogColor, kFogColor.r, kFogColor.g, kFogColor.b);
  glUniform1f(groundU_.fogStart, kFogStart);
  glUniform1f(groundU_.fogEnd,   kFogEnd);
  groundMesh_             = createGrassMesh();
  grassTex_.color         = loadTexture("data/textures/grass_color.png");
  grassTex_.normal        = loadTexture("data/textures/grass_normal.png");
  grassTex_.ao            = loadTexture("data/textures/grass_ambient_occlusion.png");
  grassTex_.roughness     = loadTexture("data/textures/grass_roughness.png");
  grassTex_.displacement  = loadTexture("data/textures/grass_displacement.png");

  // Set fog on obj + tree shader once (persists across frames)
  for (GLuint sh : {objShader_, treeShader_}) {
    glUseProgram(sh);
    glUniform3f(glGetUniformLocation(sh, "fogColor"), kFogColor.r, kFogColor.g, kFogColor.b);
    glUniform1f(glGetUniformLocation(sh, "fogStart"), kFogStart);
    glUniform1f(glGetUniformLocation(sh, "fogEnd"),   kFogEnd);
  }

  // ---- efeitos visuais ----
  particleShader_ = createShaderProgram("data/shaders/particle.vert", "data/shaders/particle.frag");
  smoke_    = std::make_unique<CannonSmoke>();
  smokeTex_ = loadTexture("data/textures/smoke.png");
  arrowMesh_ = loadMesh("data/models/projectiles/arrow/source/arrow.obj");
  arrowTex_  = loadTexture(
      "data/models/projectiles/arrow/textures/arrow_obj_initialShadingGroup_BaseColor.png");

  // ---- path de terra ----
  pathShader_ = createShaderProgram("data/shaders/path.vert", "data/shaders/path.frag");
  pathU_      = makePathUniforms(pathShader_);
  noiseTex_   = loadTexture("data/textures/perlin_noise.jpg", 3);
  dirtTex_.color        = loadTexture("data/textures/dirt_color.png");
  dirtTex_.normal       = loadTexture("data/textures/dirt_normal.png");
  dirtTex_.ao           = loadTexture("data/textures/dirt_ambient_occlusion.png");
  dirtTex_.roughness    = loadTexture("data/textures/dirt_roughness.png");
  dirtTex_.displacement = loadTexture("data/textures/dirt_displacement.png");
  glUseProgram(pathShader_);
  glUniform1i(pathU_.dirt,  0);
  glUniform1i(pathU_.noise, 1);
  glUniform1i(glGetUniformLocation(pathShader_, "normalMap"),        2);
  glUniform1i(glGetUniformLocation(pathShader_, "aoMap"),            3);
  glUniform1i(glGetUniformLocation(pathShader_, "roughnessMap"),     4);
  glUniform1i(glGetUniformLocation(pathShader_, "displacementMap"),  5);
  glUniform3f(pathU_.fogColor, kFogColor.r, kFogColor.g, kFogColor.b);
  glUniform1f(pathU_.fogStart, kFogStart);
  glUniform1f(pathU_.fogEnd,   kFogEnd);

  // Curva simples do spawn de zumbis até o castelo
  {
    std::vector<Point> pts = {
        { 1.5f,  30.0f}, { 3.0f,  20.0f}, {-1.0f,  10.0f},
        { 1.0f,   2.0f}, {-0.5f,  -7.0f}, { 1.0f, -16.0f}, { 0.5f, -24.0f},
    };
    auto curve = generateCatmullRomVertices(pts);
    pathMesh_  = std::make_unique<Mesh>(generatePathMesh(curve, 2.0f));
  }

  // ---- trees (manual placement around battle perimeter) ----
  treeLogTex_    = loadTexture("data/textures/log.jpeg");
  treeLeavesTex_ = loadTexture("data/textures/leaves.png", 4);
  treeLogMesh_    = loadMesh("data/models/world/log.obj");
  treeLeavesMesh_ = loadMesh("data/models/world/leaves.obj");

  // Ring of trees well outside the battle corridor (X ∈ [-6,6], Z ∈ [-5,25])
  struct TPos { float x, z, rot, sc; };
  static const TPos kTrees[] = {
      // left flank
      {-9,   2,  0.3f, 1.1f}, {-11,  8, 1.1f, 0.9f}, {-10, 15, 2.3f, 1.2f},
      {-12, 22,  0.7f, 1.0f}, {-8,  28, 1.8f, 0.85f},
      // right flank — kept clear of shot-0 camera path (x≈12, z≈-2..8)
      { 9,   5,  2.1f, 1.0f}, { 11,  9, 0.5f, 1.15f}, { 10, 16, 1.5f, 0.9f},
      { 12, 23,  2.7f, 1.1f}, {  8, 29, 0.2f, 1.0f},
      // behind archers — clear of shot-2 camera (x≈-3.5, z≈-7)
      {-7, -11,  1.2f, 1.2f}, { 2, -13, 2.5f, 1.0f}, { 6, -10, 0.8f, 0.95f},
      // far behind zombie spawn
      {-3,  33,  0.9f, 1.1f}, { 3,  34, 2.0f, 1.0f}, { 0,  32, 1.5f, 0.9f},
  };
  for (auto &t : kTrees) {
    TreeInstance ti;
    ti.position  = Vector<3>{t.x, 0.0f, t.z};
    ti.rotationY = t.rot;
    ti.scale     = t.sc;
    trees_.push_back(ti);
  }
  if (treeLogMesh_ && treeLeavesMesh_) {
    treeInstanceVBO_ = setupTreeInstancing(trees_, treeLogMesh_.get(), treeLeavesMesh_.get());
  }

  // ---- archer ----
  archerModel_ = std::make_unique<AnimatedModel>("data/models/archer/archer_t.glb");
  archerModel_->loadAnimation("idle", "data/models/archer/idle1.glb");
  archerModel_->loadAnimation("aim",  "data/models/archer/aim_draw.glb");
  archerTex_    = loadTexture("data/textures/archer.png");
  archerNormal_ = loadTexture("data/textures/archer_normal.png");
  bowTex_       = loadTexture("data/textures/bow.jpg");
  bowMesh_      = loadMesh("data/models/archer/bow.obj");
  archerProxy_  = std::make_unique<GameObject>(nullptr, Vector<3>{0,0,0});
  archerProxy_->model = archerModel_.get();
  archerProxy_->scale = kActorScale;

  // ---- arquebus ----
  arquebusModel_ = std::make_unique<AnimatedModel>("data/models/arquebus/arquebus_t.glb");
  arquebusModel_->loadAnimation("idle", "data/models/arquebus/arquebus_idle.glb");
  arquebusModel_->loadAnimation("fire", "data/models/arquebus/arquebus_fire.glb");
  arquebusTex_       = loadTexture("data/textures/arquebus.png");
  arquebusNormal_    = loadTexture("data/textures/arquebus_normal.png");
  arquebusWeaponTex_ = loadTexture("data/textures/arquebus_weapon.png");
  arquebusMesh_      = loadMesh("data/models/arquebus/arquebus_weapon.obj");
  arquebusProxy_     = std::make_unique<GameObject>(nullptr, Vector<3>{0,0,0});
  arquebusProxy_->model = arquebusModel_.get();
  arquebusProxy_->scale = kActorScale;

  // ---- knight ----
  knightModel_ = std::make_unique<AnimatedModel>("data/models/knight/knight_t.glb");
  knightModel_->loadAnimation("idle",    "data/models/knight/knight_idle.glb");
  knightModel_->loadAnimation("attack",  "data/models/knight/knight_attack.glb");
  knightModel_->loadAnimation("command", "data/models/knight/knight_command.glb");
  knightTex_     = loadTexture("data/textures/knight.png");
  knightNormal_  = loadTexture("data/textures/knight_normal.png");
  attachmentTex_ = loadTexture("data/textures/knight_attachment.png");
  swordMesh_     = loadMesh("data/models/knight/sword.obj");
  shieldMesh_    = loadMesh("data/models/knight/shield.obj");
  knightProxy_   = std::make_unique<GameObject>(nullptr, Vector<3>{0,0,0});
  knightProxy_->model = knightModel_.get();
  knightProxy_->scale = kActorScale;

  // ---- byzantine general ----
  byzantineModel_ = std::make_unique<AnimatedModel>("data/models/generals/byzantineT.glb");
  byzantineModel_->loadAnimation("idle",    "data/models/generals/byzantine_idle.glb");
  byzantineModel_->loadAnimation("command", "data/models/knight/knight_command.glb");
  byzantineTex_    = loadTexture("data/textures/byzantine.png");
  byzantineNormal_ = createDefaultNormalTexture();
  byzantineProxy_  = std::make_unique<GameObject>(nullptr, Vector<3>{0,0,0});
  byzantineProxy_->model = byzantineModel_.get();
  byzantineProxy_->scale = kActorScale;

  // ---- zombie ----
  zombieModel_ = std::make_unique<AnimatedModel>("data/models/zombie/zombie_t.glb");
  zombieModel_->loadAnimation("run",   "data/models/zombie/zombie_run.glb");
  zombieModel_->loadAnimation("death", "data/models/zombie/zombie_death.glb");
  zombieTex_    = loadTexture("data/textures/zombie.png");
  zombieNormal_ = createDefaultNormalTexture();
  zombieProxy_  = std::make_unique<GameObject>(nullptr, Vector<3>{0,0,0});
  zombieProxy_->model = zombieModel_.get();
  zombieProxy_->scale = kActorScale;

  // ---- place archers (2, flanks) ----
  for (float ax : {-5.5f, 5.5f}) {
    ArcherActor a; a.x = ax; a.z = -5.0f; a.idleCycle = ax * 0.25f;
    archers_.push_back(a);
  }

  // ---- place arquebus (2, center-back) ----
  float arqCooldowns[] = {4.2f, 6.8f};
  int qi = 0;
  for (float qx : {-2.0f, 2.0f}) {
    AquebusActor q; q.x = qx; q.z = -5.0f;
    q.cooldown  = arqCooldowns[qi++];
    q.animTime  = qx * 0.3f;
    arquebus_.push_back(q);
  }

  // ---- place knights (2, front/melee) ----
  for (float kx : {-2.5f, 2.5f}) {
    KnightActor k; k.x = kx; k.z = -2.0f; k.animTime = kx * 0.2f;
    knights_.push_back(k);
  }

  // ---- general (comandante no centro da formação) ----
  general_ = GeneralActor{};
  general_.commandTimer = 4.0f;

  // ---- place zombies ----
  struct ZPos { float x, z; };
  static const ZPos kZombies[] = {
      {-2.0f,  8.0f}, { 1.5f, 12.0f}, {-3.5f, 15.5f},
      { 3.0f, 19.0f}, { 0.0f, 22.5f}, {-1.0f, 26.0f},
  };
  for (auto &zi : kZombies) {
    ZombieActor z;
    z.x = zi.x; z.startX = zi.x;
    z.z = zi.z; z.startZ = zi.z;
    z.animTime = zi.z * 0.06f;
    zombies_.push_back(z);
  }

  // ---- castle ----
  {
    std::vector<ObjMaterialGroup> matGroups;
    loadObjMaterials("data/models/world/castle.obj", matGroups);
    auto mtl = parseMtlFromObj("data/models/world/castle.obj");
    castleDefaultNormal_ = createDefaultNormalTexture();
    for (auto &g : matGroups) {
      if (g.vertices.empty()) continue;
      CastleGroup cg;
      cg.mesh = std::make_unique<Mesh>(g.vertices);
      auto it = mtl.find(g.material);
      if (it != mtl.end() && !it->second.map_Kd.empty())
        cg.colorTex = loadTexture(it->second.map_Kd.c_str());
      else
        cg.colorTex = createSolidColorTexture(0.8f, 0.8f, 0.8f);
      cg.normalTex = castleDefaultNormal_;
      castleGroups_.push_back(std::move(cg));
    }
  }

  // ---- torches (4 de batalha + 2 junto ao castelo) ----
  torchLights_.resize(6);

  // ---- moonlight — ambiente alto/suave, difusa moderada, luz mais de cima ----
  moonLight_.direction = glm::normalize(glm::vec3(-0.2f, 2.8f, 0.3f));  // bem de cima, menos frontal
  moonLight_.ambient   = glm::vec3(0.38f, 0.40f, 0.52f);  // alto: ilumina tudo uniformemente
  moonLight_.diffuse   = glm::vec3(0.48f, 0.52f, 0.66f);  // moderado: sem sombras duras na cara
  moonLight_.specular  = glm::vec3(0.16f, 0.18f, 0.26f);  // baixo: sem brilho excessivo

  // ---- fullscreen quad ----
  glGenVertexArrays(1, &quadVAO_);
  glGenBuffers(1, &quadVBO_);
  glBindVertexArray(quadVAO_);
  glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
  glBindVertexArray(0);

  fadeAlpha_ = 1.0f;

  // ---- música do menu ----
  audio::playMusic("data/audio/menu.mp3", 0.35f);

  // ---- fonte do título (Cinzel 48pt) ----
  {
    ImGuiIO &io = ImGui::GetIO();
    titleFont_ = io.Fonts->AddFontFromFileTTF(
        "data/fonts/Cinzel-VariableFont_wght.ttf", 48.0f);
    // Backend com ImGuiBackendFlags_RendererHasTextures reconstrói o atlas automaticamente
  }
}

// =============================================================================
// UPDATE
// =============================================================================

void MainMenu::updateScene(float dt) {
  sceneTime_ += dt;

  // ---- Shot sequencing ----
  const float dur = kShots[shot_].duration;
  shotTimer_ += dt;
  if (shotTimer_ < kFadeDuration)
    fadeAlpha_ = 1.0f - shotTimer_ / kFadeDuration;
  else if (shotTimer_ > dur - kFadeDuration)
    fadeAlpha_ = (shotTimer_ - (dur - kFadeDuration)) / kFadeDuration;
  else
    fadeAlpha_ = 0.0f;
  fadeAlpha_ = glm::clamp(fadeAlpha_, 0.0f, 1.0f);
  if (shotTimer_ >= dur) { shot_ = (shot_ + 1) % 3; shotTimer_ = 0.0f; }

  // ---- Torch flicker ----
  auto flicker = [&](float seed) {
    return 0.82f + 0.18f * std::sin(sceneTime_ * 7.1f + seed)
                 + 0.06f * std::sin(sceneTime_ * 13.3f + seed * 2.3f);
  };
  const glm::vec3 torchColor = glm::vec3(1.1f, 0.5f, 0.08f);
  // Archotes laterais elevados — iluminam de cima/lado, não na cara
  torchLights_[0].position = glm::vec3(-6.0f, 4.5f, -3.0f);
  torchLights_[0].color    = torchColor * 5.5f * flicker(0.0f);
  torchLights_[1].position = glm::vec3( 6.0f, 4.5f, -3.0f);
  torchLights_[1].color    = torchColor * 5.5f * flicker(1.7f);
  torchLights_[2].position = glm::vec3(-6.5f, 4.0f,  4.0f);
  torchLights_[2].color    = torchColor * 4.0f * flicker(3.1f);
  torchLights_[3].position = glm::vec3( 6.5f, 4.0f,  4.0f);
  torchLights_[3].color    = torchColor * 4.0f * flicker(4.9f);
  // Archotes do castelo ao fundo
  torchLights_[4].position = glm::vec3(-5.0f, 6.0f, -20.0f);
  torchLights_[4].color    = torchColor * 5.5f * flicker(6.3f);
  torchLights_[5].position = glm::vec3( 5.0f, 6.0f, -20.0f);
  torchLights_[5].color    = torchColor * 5.5f * flicker(8.1f);

  // ---- Zombies: walk + die ----
  for (auto &z : zombies_) {
    if (z.dying) {
      z.animTime   += dt;
      z.deathTimer += dt;
      if (z.deathTimer >= kDeathDuration) {
        z.z = z.startZ; z.x = z.startX;
        z.animTime = 0.0f; z.dying = false; z.deathTimer = 0.0f;
      }
    } else {
      z.z        -= kZombieSpeed * dt;
      z.animTime += dt;

      // Knight melee range check
      for (auto &k : knights_) {
        if (!k.attacking &&
            z.z <= k.z + kKnightRange &&
            std::abs(z.x - k.x) < 3.0f) {
          z.dying    = true;
          z.animTime = 0.0f;
          k.attacking  = true;
          k.attackTimer = kAttackDuration;
          k.animTime   = 0.0f;
          break;
        }
      }

      if (z.z <= kZombieDeathZ && !z.dying) {
        z.dying = true; z.animTime = 0.0f;
      }
    }
  }

  // ---- Scripted archer kill (periodic) ----
  killTimer_ -= dt;
  if (killTimer_ <= 0.0f) {
    int best = -1; float bestZ = 1e9f;
    for (int i = 0; i < (int)zombies_.size(); ++i) {
      if (!zombies_[i].dying && zombies_[i].z > -1.0f && zombies_[i].z < bestZ) {
        bestZ = zombies_[i].z; best = i;
      }
    }
    if (best >= 0) {
      float bx = zombies_[best].x;
      float bz = zombies_[best].z;
      zombies_[best].dying = true; zombies_[best].animTime = 0.0f;
      // Pick nearest archer
      int na = 0; float nd = 1e9f;
      for (int i = 0; i < (int)archers_.size(); ++i) {
        float d = std::abs(archers_[i].x - bx);
        if (d < nd) { nd = d; na = i; }
      }
      archers_[na].aiming   = true;
      archers_[na].aimTimer = kAimDuration;
      archers_[na].animTime = 0.0f;
      // Spawna flecha do arco rumo ao zumbi
      {
        glm::vec3 origin = {archers_[na].x, 1.3f, archers_[na].z};
        glm::vec3 target = {bx, 0.8f, bz};
        glm::vec3 dir    = glm::normalize(target - origin);
        ArrowProjectile arrow;
        arrow.position  = origin;
        arrow.velocity  = dir * kArrowSpeed;
        arrow.maxDist   = glm::length(target - origin) * 1.15f;
        arrow.targetIdx = -1;
        menuArrows_.push_back(arrow);
      }
    }
    killTimer_ = 3.5f + static_cast<float>(rand() % 300) / 100.0f;
  }

  // ---- Archer animation ----
  for (auto &a : archers_) {
    a.animTime += dt;
    if (a.aiming) {
      a.aimTimer -= dt;
      if (a.aimTimer <= 0.0f) { a.aiming = false; a.animTime = a.idleCycle; }
    }
  }

  // ---- Arquebus animation (periodic fire) ----
  for (auto &q : arquebus_) {
    q.animTime += dt;
    if (q.firing) {
      q.fireTimer -= dt;
      if (q.fireTimer <= 0.0f) {
        q.firing   = false;
        q.cooldown = 5.0f + static_cast<float>(rand() % 300) / 100.0f;
        // Não reseta animTime — idle continua fluidamente sem pop de pose
      }
    } else {
      q.cooldown -= dt;
      if (q.cooldown <= 0.0f) {
        q.firing    = true;
        q.fireTimer = kFireDuration;
        q.animTime  = 0.0f;
        // Fumaça no cano (arcabuz aponta para +Z, cano a ~1.2 unidades à frente)
        if (smoke_)
          smoke_->emit(glm::vec3(q.x, 1.2f, q.z + 1.2f), 4, 0.35f);
      }
    }
  }

  // ---- Knight animation ----
  for (auto &k : knights_) {
    k.animTime += dt;
    if (k.attacking) {
      k.attackTimer -= dt;
      if (k.attackTimer <= 0.0f) { k.attacking = false; k.animTime = 0.0f; }
    }
  }

  // ---- General (comandante) — ocasionalmente executa animação de comando ----
  general_.animTime += dt;
  if (general_.commanding) {
    general_.commandDuration -= dt;
    if (general_.commandDuration <= 0.0f) {
      general_.commanding = false;
      general_.animTime   = 0.0f;
      general_.commandTimer = 5.0f + static_cast<float>(rand() % 500) / 100.0f;
    }
  } else {
    general_.commandTimer -= dt;
    if (general_.commandTimer <= 0.0f) {
      general_.commanding      = true;
      general_.animTime        = 0.0f;
      general_.commandDuration = 3.2f;
    }
  }

  // ---- Flechas em voo ----
  for (auto &a : menuArrows_) {
    a.position += a.velocity * dt;
    a.traveled += glm::length(a.velocity) * dt;
  }
  menuArrows_.erase(
      std::remove_if(menuArrows_.begin(), menuArrows_.end(),
          [](const ArrowProjectile &a) { return a.traveled >= a.maxDist; }),
      menuArrows_.end());

  // ---- Fumaça ----
  if (smoke_) smoke_->update(dt);
}

// =============================================================================
// RENDER
// =============================================================================

void MainMenu::renderScene(int fbW, int fbH) {
  using namespace render_constants;
  using namespace math_constants;

  ensureFbo(fbW, fbH);

  const CinematicShot &cs = kShots[shot_];
  float t = glm::clamp(shotTimer_ / cs.duration, 0.0f, 1.0f);
  glm::vec3 camPos = glm::mix(cs.camStart, cs.camEnd, t);
  glm::vec3 lookAt = glm::mix(cs.lookStart, cs.lookEnd, t);

  const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
  cam_.setPerspective(glm::radians(cs.fovDeg), aspect, kNearPlane, kFarPlane);
  cam_.setLookAt(Vector<3>{camPos.x, camPos.y, camPos.z},
                 Vector<3>{lookAt.x,  lookAt.y,  lookAt.z});

  const auto      glView  = toOpenGLMatrix(cam_.getViewMatrix());
  const auto      glProj  = toOpenGLMatrix(cam_.getProjectionMatrix());
  const glm::vec3 viewPos = camPos;

  // ===== SCENE → FBO =========================================================
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glViewport(0, 0, fbW, fbH);
  glEnable(GL_DEPTH_TEST);
  glClearColor(kFogColor.r * 0.3f, kFogColor.g * 0.3f, kFogColor.b * 0.5f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // -- Ground (grama estática) -----------------------------------------------
  uploadCommonUniforms(groundShader_, moonLight_, torchLights_, viewPos, glView, glProj);
  renderGround(groundMesh_, grassTex_, noiseTex_);

  // -- Path de terra ----------------------------------------------------------
  if (pathMesh_) {
    uploadCommonUniforms(pathShader_, moonLight_, torchLights_, viewPos, glView, glProj);
    renderPath(pathU_, *pathMesh_, dirtTex_, noiseTex_, Matrix<4,4>::identity());
  }

  // -- Trees ------------------------------------------------------------------
  if (treeLogMesh_ && treeLeavesMesh_) {
    uploadCommonUniforms(treeShader_, moonLight_, torchLights_, viewPos, glView, glProj);
    renderTrees(treeShader_, trees_,
                treeLogMesh_.get(), treeLogTex_,
                treeLeavesMesh_.get(), treeLeavesTex_);
  }

  // -- Upload anim shader uniforms for all animated bodies -------------------
  uploadCommonUniforms(animShader_, moonLight_, torchLights_, viewPos, glView, glProj);
  glUseProgram(animShader_);

  // -- Archers ----------------------------------------------------------------
  for (auto &a : archers_) {
    archerProxy_->position      = Vector<3>{a.x, 0.0f, a.z};
    archerProxy_->rotationY     = 0.0f;
    archerProxy_->currentAnimation = a.aiming ? "aim" : "idle";
    archerProxy_->animationTime = a.animTime;
    archerProxy_->loopAnim      = !a.aiming;
    drawBody(animShader_, *archerProxy_, archerTex_, archerNormal_);
  }

  // -- Arquebus ---------------------------------------------------------------
  for (auto &q : arquebus_) {
    arquebusProxy_->position      = Vector<3>{q.x, 0.0f, q.z};
    arquebusProxy_->rotationY     = 0.0f;
    arquebusProxy_->currentAnimation = q.firing ? "fire" : "idle";
    arquebusProxy_->animationTime = q.animTime;
    arquebusProxy_->loopAnim      = !q.firing;
    drawBody(animShader_, *arquebusProxy_, arquebusTex_, arquebusNormal_);
  }

  // -- Knights ----------------------------------------------------------------
  for (auto &k : knights_) {
    knightProxy_->position      = Vector<3>{k.x, 0.0f, k.z};
    knightProxy_->rotationY     = 0.0f;
    knightProxy_->currentAnimation = k.attacking ? "attack" : "idle";
    knightProxy_->animationTime = k.animTime;
    knightProxy_->loopAnim      = !k.attacking;
    drawBody(animShader_, *knightProxy_, knightTex_, knightNormal_);
  }

  // -- General Bizantino (centro da formação) ---------------------------------
  byzantineProxy_->position      = Vector<3>{0.0f, 0.0f, -3.5f};
  byzantineProxy_->rotationY     = 0.0f;
  byzantineProxy_->currentAnimation = general_.commanding ? "command" : "idle";
  byzantineProxy_->animationTime = general_.animTime;
  byzantineProxy_->loopAnim      = !general_.commanding;
  drawBody(animShader_, *byzantineProxy_, byzantineTex_, byzantineNormal_);

  // -- Zombies ----------------------------------------------------------------
  for (auto &z : zombies_) {
    zombieProxy_->position      = Vector<3>{z.x, 0.0f, z.z};
    zombieProxy_->rotationY     = kPi;
    zombieProxy_->currentAnimation = z.dying ? "death" : "run";
    zombieProxy_->animationTime = z.animTime;
    zombieProxy_->loopAnim      = !z.dying;
    drawBody(animShader_, *zombieProxy_, zombieTex_, zombieNormal_);
  }

  // -- Weapons (OBJ meshes attached to bones) ---------------------------------
  if (bowMesh_ || arquebusMesh_ || swordMesh_) {
    uploadCommonUniforms(objShader_, moonLight_, torchLights_, viewPos, glView, glProj);
    glUseProgram(objShader_);

    const glm::mat4 bowOff = makeBowOffset();
    const glm::mat4 arqOff = makeArqOffset();
    const glm::mat4 swdOff = makeSwordOffset();
    const glm::mat4 shdOff = makeShieldOffset();

    if (bowMesh_) {
      for (auto &a : archers_) {
        archerProxy_->position      = Vector<3>{a.x, 0.0f, a.z};
        archerProxy_->rotationY     = 0.0f;
        archerProxy_->currentAnimation = a.aiming ? "aim" : "idle";
        archerProxy_->animationTime = a.animTime;
        archerProxy_->loopAnim      = !a.aiming;
        drawWeapon(objShader_, objU_, *archerProxy_,
                   "mixamorig:LeftHand", *bowMesh_, bowTex_, bowOff);
      }
    }

    if (arquebusMesh_) {
      for (auto &q : arquebus_) {
        arquebusProxy_->position      = Vector<3>{q.x, 0.0f, q.z};
        arquebusProxy_->rotationY     = 0.0f;
        arquebusProxy_->currentAnimation = q.firing ? "fire" : "idle";
        arquebusProxy_->animationTime = q.animTime;
        arquebusProxy_->loopAnim      = !q.firing;
        drawWeapon(objShader_, objU_, *arquebusProxy_,
                   "mixamorig:LeftHand", *arquebusMesh_, arquebusWeaponTex_, arqOff);
      }
    }

    if (swordMesh_) {
      for (auto &k : knights_) {
        knightProxy_->position      = Vector<3>{k.x, 0.0f, k.z};
        knightProxy_->rotationY     = 0.0f;
        knightProxy_->currentAnimation = k.attacking ? "attack" : "idle";
        knightProxy_->animationTime = k.animTime;
        knightProxy_->loopAnim      = !k.attacking;
        drawWeapon(objShader_, objU_, *knightProxy_,
                   "mixamorig:RightHand", *swordMesh_, attachmentTex_, swdOff);
        if (shieldMesh_) {
          drawWeapon(objShader_, objU_, *knightProxy_,
                     "mixamorig:LeftHand", *shieldMesh_, attachmentTex_, shdOff);
        }
      }
      // General também carrega a espada na mão direita (proxy já tem pos/anim do drawBody)
      drawWeapon(objShader_, objU_, *byzantineProxy_,
                 "mixamorig:RightHand", *swordMesh_, attachmentTex_, swdOff);
    }
  }

  // -- Castelo ao fundo -------------------------------------------------------
  if (!castleGroups_.empty()) {
    uploadCommonUniforms(objShader_, moonLight_, torchLights_, viewPos, glView, glProj);
    glUseProgram(objShader_);

    Matrix<4,4> t = translate<4,4>(3.0f, 0.0f, -33.0f);
    Matrix<4,4> r = rotateY<4,4>(200.0f * kDegToRad);
    Matrix<4,4> s = ::scale<4,4>(0.9f, 0.9f, 0.9f);
    auto glM = toOpenGLMatrix(t * r * s);
    glUniformMatrix4fv(objU_.model, 1, GL_FALSE, glM.data());

    for (const auto &g : castleGroups_) {
      if (!g.mesh) continue;
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, g.colorTex);
      glUniform1i(cachedUniformLocation(objShader_, "tex"), 0);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, g.normalTex);
      glUniform1i(cachedUniformLocation(objShader_, "normalMap"), 1);
      g.mesh->draw();
    }
  }

  // -- Flechas ----------------------------------------------------------------
  if (!menuArrows_.empty() && arrowMesh_) {
    uploadCommonUniforms(objShader_, moonLight_, torchLights_, viewPos, glView, glProj);
    renderArrows(objShader_, objU_, menuArrows_, *arrowMesh_, arrowTex_);
  }

  // -- Fumaça dos arcabuzes --------------------------------------------------
  if (smoke_) {
    const glm::vec3 camRight(glView[0], glView[4], glView[8]);
    const glm::vec3 camUp   (glView[1], glView[5], glView[9]);
    smoke_->render(particleShader_, smokeTex_, camRight, camUp, glView, glProj);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // ===== POST PASS ===========================================================
  glDisable(GL_DEPTH_TEST);
  glViewport(0, 0, fbW, fbH);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(postShader_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, fboColorTex_);
  glUniform1i(cachedUniformLocation(postShader_, "screenTex"),    0);
  glUniform1f(cachedUniformLocation(postShader_, "time"),         sceneTime_);
  glUniform1f(cachedUniformLocation(postShader_, "fadeAlpha"),    fadeAlpha_);
  glUniform1f(cachedUniformLocation(postShader_, "screenAspect"), aspect);
  glBindVertexArray(quadVAO_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
  glEnable(GL_DEPTH_TEST);
}

// =============================================================================
// UNLOAD
// =============================================================================

void MainMenu::unloadScene() {
  for (GLuint sh : {groundShader_, pathShader_, animShader_, objShader_,
                    treeShader_, postShader_, particleShader_}) {
    if (sh) { invalidateUniformLocationCache(sh); glDeleteProgram(sh); }
  }
  groundShader_ = pathShader_ = animShader_ = objShader_ =
      treeShader_ = postShader_ = particleShader_ = 0;
  smoke_.reset();
  arrowMesh_.reset();
  deleteGpuMesh(groundMesh_);
  for (GLuint t : {grassTex_.color, grassTex_.normal, grassTex_.ao,
                   grassTex_.roughness, grassTex_.displacement,
                   dirtTex_.color, dirtTex_.normal, dirtTex_.ao,
                   dirtTex_.roughness, dirtTex_.displacement,
                   noiseTex_, smokeTex_, arrowTex_,
                   archerTex_, archerNormal_, bowTex_,
                   arquebusTex_, arquebusNormal_, arquebusWeaponTex_,
                   knightTex_, knightNormal_, attachmentTex_,
                   zombieTex_, zombieNormal_,
                   treeLogTex_, treeLeavesTex_,
                   byzantineTex_, byzantineNormal_}) {
    if (t) glDeleteTextures(1, &t);
  }
  for (auto &g : castleGroups_) {
    if (g.colorTex) glDeleteTextures(1, &g.colorTex);
  }
  castleGroups_.clear();
  if (castleDefaultNormal_) { glDeleteTextures(1, &castleDefaultNormal_); castleDefaultNormal_ = 0; }

  audio::stopMusic();
  pathMesh_.reset();
  if (treeInstanceVBO_) glDeleteBuffers(1, &treeInstanceVBO_);
  treeInstanceVBO_ = 0;

  if (fbo_) {
    glDeleteFramebuffers(1, &fbo_);
    glDeleteTextures(1, &fboColorTex_);
    glDeleteRenderbuffers(1, &fboDepthRbo_);
    fbo_ = fboColorTex_ = fboDepthRbo_ = 0;
  }
  glDeleteVertexArrays(1, &quadVAO_);
  glDeleteBuffers(1, &quadVBO_);
  quadVAO_ = quadVBO_ = 0;
  fboW_ = fboH_ = 0;

  // ---- fontes ----
  {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    // Restaura Cinzel 18pt como default (igual ao Hud::init) para o restante do jogo
    io.Fonts->AddFontFromFileTTF("data/fonts/Cinzel-VariableFont_wght.ttf", 18.0f);
    titleFont_ = nullptr;
  }
}

// =============================================================================
// UI
// =============================================================================

void MainMenu::renderUI(float W, float H, bool &outEnter, bool &outQuit) {
  // Vignette escura sutil sobre a cena 3D
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(W, H));
  ImGui::SetNextWindowBgAlpha(0.0f);
  ImGui::Begin("##mm_vignette", nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(0, 0), ImVec2(W, H), IM_COL32(4, 6, 15, 90));
  ImGui::End();

  // Título e créditos via foreground draw list (sem janela interativa)
  ImDrawList *dl = ImGui::GetForegroundDrawList();

  // Título "1346AD: BLOOD & IRON" centrado no topo
  if (titleFont_) {
    constexpr float kTitleSize = 48.0f;
    const char *title = "1346AD: BLOOD & IRON";
    ImVec2 tsz = titleFont_->CalcTextSizeA(kTitleSize, FLT_MAX, 0.0f, title);
    const float tx = (W - tsz.x) * 0.5f;
    const float ty = H * 0.06f;
    dl->AddText(titleFont_, kTitleSize, ImVec2(tx + 2.0f, ty + 2.0f), IM_COL32(0, 0, 0, 200), title);
    dl->AddText(titleFont_, kTitleSize, ImVec2(tx, ty),                IM_COL32(218, 175, 50, 248), title);
  }

  // Créditos no canto inferior esquerdo — fonte padrão ImGui, boa visibilidade
  {
    const float fs  = ImGui::GetFontSize();
    const float y0  = H - fs * 2.0f - 20.0f;
    const ImU32 col = IM_COL32(210, 195, 162, 220);
    dl->AddText(ImVec2(16.0f, y0),          col, "Caetano Meneghetti (00591004)");
    dl->AddText(ImVec2(16.0f, y0 + fs + 4.0f), col, "Fernando Tedesco (00591001)");
  }

  // Dois botões minimalistas centrados no terço inferior da tela
  const float btnW = 190.0f, btnH = 46.0f, gap = 12.0f;
  ImGui::SetNextWindowPos(ImVec2(W * 0.5f, H * 0.88f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowBgAlpha(0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      ImVec2(gap, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,  1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,    2.0f);
  ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.05f, 0.04f, 0.02f, 0.80f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.22f, 0.04f, 0.90f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.50f, 0.32f, 0.06f, 1.00f));
  ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.90f, 0.78f, 0.48f, 1.00f));
  ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(0.55f, 0.42f, 0.15f, 0.65f));
  ImGui::Begin("##btns", nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoBringToFrontOnFocus);
  if (ImGui::Button("JOGAR", ImVec2(btnW, btnH))) outEnter = true;
  ImGui::SameLine();
  if (ImGui::Button("SAIR",  ImVec2(btnW, btnH))) outQuit  = true;
  ImGui::End();
  ImGui::PopStyleColor(5);
  ImGui::PopStyleVar(5);
}

// =============================================================================
// RUN
// =============================================================================

bool MainMenu::run(GLFWwindow *window) {
  loadScene();
  double prevTime = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    const double now = glfwGetTime();
    const float  dt  = static_cast<float>(now - prevTime);
    prevTime = now;
    glfwPollEvents();

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    updateScene(dt);

    glViewport(0, 0, fbW, fbH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderScene(fbW, fbH);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    bool enterClicked = false, quitClicked = false;
    renderUI(static_cast<float>(fbW), static_cast<float>(fbH), enterClicked, quitClicked);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);

    if (enterClicked) {
      audio::playOneShot("data/audio/start.mp3");
      // Fade-out da música enquanto a cena continua renderizando (~1.5s)
      constexpr float kFadeOut = 1.5f;
      float fadeTimer = 0.0f;
      double fadePrev = glfwGetTime();
      while (fadeTimer < kFadeOut && !glfwWindowShouldClose(window)) {
        const double now2 = glfwGetTime();
        const float  dt2  = static_cast<float>(now2 - fadePrev);
        fadePrev = now2;
        fadeTimer += dt2;
        audio::setMusicVolume(1.0f - fadeTimer / kFadeOut);
        glfwPollEvents();
        int fbW2, fbH2;
        glfwGetFramebufferSize(window, &fbW2, &fbH2);
        updateScene(dt2);
        glViewport(0, 0, fbW2, fbH2);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderScene(fbW2, fbH2);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
      }
      unloadScene();
      return true;
    }
    if (quitClicked)  { unloadScene(); return false; }
  }

  unloadScene();
  return false;
}
