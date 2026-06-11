// =============================================================================
// 1346AD: IRON & BLOOD
// =============================================================================

// ---- 3rd-party ----
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// ---- std ----
#include <cmath>
#include <ctime>
#include <iostream>
#include <memory>
#include <vector>

// ---- engine ----
#include "engine/audio.h"
#include "engine/animated_model.h"
#include "engine/camera.h"
#include "engine/catmull_rom.h"
#include "engine/game_object.h"
#include "engine/hud.h"
#include "engine/lighting.h"
#include "engine/mesh.h"
#include "engine/obj_loader.h"
#include "engine/primitive_meshes.h"
#include "engine/shader_program.h"
#include "engine/texture_loader.h"

// ---- math ----
#include "math/constants.h"
#include "math/matrix.h"
#include "math/matrix_ops.h"
#include "math/opengl_utils.h"
#include "math/transforms.h"
#include "math/vector.h"

// ---- game ----
#include "game/app_state.h"
#include "game/defender_system.h"
#include "game/enemy_system.h"
#include "game/game_constants.h"
#include "game/path_navigation.h"
#include "game/scene.h"
#include "game/spell_effects.h"
#include "game/troop_placement.h"
#include "game/troop_selection.h"
#include "game/upgrade_system.h"
#include "game/knight_system.h"
#include "game/wave_system.h"

// ---- input ----
#include "input/camera_controller.h"
#include "input/input_handler.h"

// ---- spell ----
#include "spell/shape_classifier.h"
#include "spell/spell_mode.h"

// ---- render ----
#include "render/particle_system.h"
#include "render/render_constants.h"
#include "render/scene_renderer.h"
#include "render/shader_uniforms.h"

// ---- world ----
#include "world/path_generator.h"

namespace {

// =============================================================================
// SHADER PIPELINE
// =============================================================================
// Estrutura para agrupar shaders e seus uniform caches.
struct ShaderPipeline {
  GLuint groundShader = 0;
  GLuint objShader = 0;
  GLuint pathShader = 0;
  GLuint lineShader = 0;
  GLuint skyShader = 0;
  GLuint lanternShader = 0;
  GLuint previewShader = 0;
  GLuint animShader = 0;
  GLuint particleShader = 0;

  GroundUniforms groundU{};
  ObjUniforms objU{};
  PathUniforms pathU{};
  LineUniforms lineU{};
  LanternUniforms lanternU{};

  GLint skyViewLoc = 0;
  GLint skyProjLoc = 0;
  GLint skyTexLoc = 0;
};

bool loadAllShaders(ShaderPipeline &p) {
  p.groundShader   = createShaderProgram("data/shaders/grass.vert", "data/shaders/grass.frag");
  p.objShader      = createShaderProgram("data/shaders/shader.vert", "data/shaders/shader.frag");
  p.pathShader     = createShaderProgram("data/shaders/path.vert", "data/shaders/path.frag");
  p.lineShader     = createShaderProgram("data/shaders/line.vert", "data/shaders/line.frag");
  p.skyShader      = createShaderProgram("data/shaders/sky.vert", "data/shaders/sky.frag");
  p.lanternShader  = createShaderProgram("data/shaders/lantern.vert", "data/shaders/lantern.frag");
  p.previewShader  = createShaderProgram("data/shaders/preview.vert", "data/shaders/preview.frag");
  p.animShader     = createShaderProgram("data/shaders/anim_shader.vert", "data/shaders/anim_shader.frag");
  p.particleShader = createShaderProgram("data/shaders/particle.vert",    "data/shaders/particle.frag");

  if (!p.groundShader || !p.objShader || !p.pathShader || !p.lineShader || !p.lanternShader) {
    std::cout << "ERRO: Falha ao criar um ou mais shaders" << std::endl;
    return false;
  }

  p.groundU = makeGroundUniforms(p.groundShader);
  p.objU = makeObjUniforms(p.objShader);
  p.pathU = makePathUniforms(p.pathShader);
  p.lineU = makeLineUniforms(p.lineShader);
  p.lanternU = makeLanternUniforms(p.lanternShader);

  p.skyViewLoc = glGetUniformLocation(p.skyShader, "view");
  p.skyProjLoc = glGetUniformLocation(p.skyShader, "projection");
  p.skyTexLoc  = glGetUniformLocation(p.skyShader, "skyTexture");

  return true;
}

void deleteAllShaders(ShaderPipeline &p) {
  glDeleteProgram(p.groundShader);
  glDeleteProgram(p.objShader);
  glDeleteProgram(p.pathShader);
  glDeleteProgram(p.lineShader);
  glDeleteProgram(p.lanternShader);
  glDeleteProgram(p.previewShader);
  glDeleteProgram(p.skyShader);
  glDeleteProgram(p.animShader);
  glDeleteProgram(p.particleShader);
}

// =============================================================================
// UNIFORMS ESTÁTICOS (texture units + fog) — setadas uma vez só.
// =============================================================================
void setStaticUniforms(const ShaderPipeline &p) {
  using namespace render_constants;
  const float fogStart = kFogStart;
  const float fogEnd = kFogEnd;
  const glm::vec3 fogColor = kFogColor;

  glUseProgram(p.groundShader);
  glUniform1i(p.groundU.grass, 0);
  glUniform1i(p.groundU.noise, 1);
  glUniform1i(glGetUniformLocation(p.groundShader, "normalMap"), 2);
  glUniform1i(glGetUniformLocation(p.groundShader, "aoMap"), 3);
  glUniform1i(glGetUniformLocation(p.groundShader, "roughnessMap"), 4);
  glUniform1i(glGetUniformLocation(p.groundShader, "displacementMap"), 5);
  glUniform3fv(p.groundU.fogColor, 1, glm::value_ptr(fogColor));
  glUniform1f(p.groundU.fogStart, fogStart);
  glUniform1f(p.groundU.fogEnd, fogEnd);

  glUseProgram(p.pathShader);
  glUniform1i(p.pathU.dirt, 0);
  glUniform1i(p.pathU.noise, 1);
  glUniform1i(glGetUniformLocation(p.pathShader, "normalMap"), 2);
  glUniform1i(glGetUniformLocation(p.pathShader, "aoMap"), 3);
  glUniform1i(glGetUniformLocation(p.pathShader, "roughnessMap"), 4);
  glUniform1i(glGetUniformLocation(p.pathShader, "displacementMap"), 5);
  glUniform3fv(p.pathU.fogColor, 1, glm::value_ptr(fogColor));
  glUniform1f(p.pathU.fogStart, fogStart);
  glUniform1f(p.pathU.fogEnd, fogEnd);

  glUseProgram(p.objShader);
  glUniform3fv(glGetUniformLocation(p.objShader, "fogColor"), 1, glm::value_ptr(fogColor));
  glUniform1f(glGetUniformLocation(p.objShader, "fogStart"), fogStart);
  glUniform1f(glGetUniformLocation(p.objShader, "fogEnd"), fogEnd);

  glUseProgram(p.lanternShader);
  glUniform1i(glGetUniformLocation(p.lanternShader, "colorMap"),     0);
  glUniform1i(glGetUniformLocation(p.lanternShader, "normalMap"),    1);
  glUniform1i(glGetUniformLocation(p.lanternShader, "roughnessMap"), 2);
  glUniform1i(glGetUniformLocation(p.lanternShader, "metallicMap"),  3);
  glUniform1i(glGetUniformLocation(p.lanternShader, "aoMap"),        4);
  glUniform1i(glGetUniformLocation(p.lanternShader, "opacityMap"),   5);
  glUniform3fv(glGetUniformLocation(p.lanternShader, "fogColor"), 1, glm::value_ptr(fogColor));
  glUniform1f(glGetUniformLocation(p.lanternShader, "fogStart"), fogStart);
  glUniform1f(glGetUniformLocation(p.lanternShader, "fogEnd"), fogEnd);
}

// =============================================================================
// BUNDLES DE TEXTURAS DA CENA
// =============================================================================
struct SceneTextures {
  GrassTextures grass;
  DirtTextures dirt;
  LanternTextures lantern;

  unsigned int noise;
  unsigned int sky;

  unsigned int enemyColor;
  unsigned int defaultNormal;

  unsigned int archerColor;
  unsigned int archerNormal;
  unsigned int bowColor;

  unsigned int arquebusColor;
  unsigned int arquebusNormal;
  unsigned int arquebusWeapon;

  unsigned int knightColor;
  unsigned int knightNormal;
  unsigned int swordColor;
  unsigned int shieldColor;

  unsigned int castleColor;
  unsigned int castleNormal;

  unsigned int treeLog;
  unsigned int treeLeaves;

  unsigned int selectionCircle;
  unsigned int armoredZombieColor;

  unsigned int cannonerColor;
  unsigned int cannonerNormal;
  unsigned int cannonBarrelColor;
  unsigned int torchColor;
  unsigned int smokeTexture;
};

SceneTextures loadAllSceneTextures() {
  SceneTextures t;
  t.grass.color        = loadTexture("data/textures/grass_color.png");
  t.grass.normal       = loadTexture("data/textures/grass_normal.png");
  t.grass.ao           = loadTexture("data/textures/grass_ambient_occlusion.png");
  t.grass.roughness    = loadTexture("data/textures/grass_roughness.png");
  t.grass.displacement = loadTexture("data/textures/grass_displacement.png");

  t.dirt.color         = loadTexture("data/textures/dirt_color.png");
  t.dirt.normal        = loadTexture("data/textures/dirt_normal.png");
  t.dirt.ao            = loadTexture("data/textures/dirt_ambient_occlusion.png");
  t.dirt.roughness     = loadTexture("data/textures/dirt_roughness.png");
  t.dirt.displacement  = loadTexture("data/textures/dirt_displacement.png");

  t.lantern.color      = loadTexture("data/textures/lantern_color.jpg");
  t.lantern.normal     = loadTexture("data/textures/lantern_normal.jpg");
  t.lantern.roughness  = loadTexture("data/textures/lantern_roughness.jpg");
  t.lantern.metallic   = loadTexture("data/textures/lantern_metallic.jpg");
  t.lantern.ao         = loadTexture("data/textures/lantern_mixed_ambient_occlusion.jpg");
  t.lantern.opacity    = loadTexture("data/textures/lantern_opacity.jpg");

  t.noise              = loadTexture("data/textures/perlin_noise.jpg", 3);
  t.sky                = loadTexture("data/textures/night_sky_tonemapped.jpg");

  t.enemyColor         = loadTexture("data/textures/zombie.png");
  t.defaultNormal      = createDefaultNormalTexture();

  t.archerColor        = loadTexture("data/textures/archer.png");
  t.archerNormal       = loadTexture("data/textures/archer_normal.png");
  t.bowColor           = loadTexture("data/textures/bow.jpg");

  t.arquebusColor      = loadTexture("data/textures/arquebus.png");
  t.arquebusNormal     = loadTexture("data/textures/arquebus_normal.png");
  t.arquebusWeapon     = loadTexture("data/textures/arquebus_weapon.png");

  t.swordColor  = loadTexture("data/textures/knight_attachment.png");
  t.shieldColor = loadTexture("data/textures/knight_attachment.png");

  t.knightColor  = loadTexture("data/textures/knight.png");
  t.knightNormal = loadTexture("data/textures/knight_normal.png");

  t.castleColor        = loadTexture("data/textures/castle.png");
  t.castleNormal       = loadTexture("data/textures/castle_normal.png");

  t.treeLog            = loadTexture("data/textures/log.jpeg");
  t.treeLeaves         = loadTexture("data/textures/leaves.png", 4);

  t.selectionCircle    = loadTexture("data/textures/selection_circle.png", 4);
  t.armoredZombieColor = loadTexture("data/textures/armor_zombie.png", 4);

  t.cannonerColor      = loadTexture("data/textures/cannoner.png");
  t.cannonerNormal     = loadTexture("data/textures/cannoner_normal.png");
  t.cannonBarrelColor  = loadTexture("data/textures/cannon.png");
  t.torchColor         = loadTexture("data/textures/torch.png");
  t.smokeTexture       = loadTexture("data/textures/smoke.png", 4);

  return t;
}

void deleteAllSceneTextures(const SceneTextures &t) {
  const unsigned int all[] = {
      t.grass.color, t.grass.normal, t.grass.ao, t.grass.roughness, t.grass.displacement,
      t.dirt.color, t.dirt.normal, t.dirt.ao, t.dirt.roughness, t.dirt.displacement,
      t.lantern.color, t.lantern.normal, t.lantern.roughness, t.lantern.metallic, t.lantern.ao, t.lantern.opacity,
      t.noise, t.sky,
      t.enemyColor, t.defaultNormal,
      t.archerColor, t.archerNormal, t.bowColor,
      t.arquebusColor, t.arquebusNormal, t.arquebusWeapon,
      t.castleColor, t.castleNormal,
      t.treeLog, t.treeLeaves,
      t.swordColor, t.shieldColor,
      t.selectionCircle, t.armoredZombieColor,
      t.cannonerColor, t.cannonerNormal, t.cannonBarrelColor, t.torchColor, t.smokeTexture,
  };
  for (unsigned int tex : all) {
    if (tex != 0) glDeleteTextures(1, &tex);
  }
}

// =============================================================================
// PONTOS DE CONTROLE DA CURVA (path do nível)
// =============================================================================
// f(x) = 10*sin(0.20x + 0.3) + 3*sin(0.40x - 0.5), x ∈ [-22, 22] em 28 pontos.
std::vector<Point> defaultControlPoints() {
  return {
      {-22.0000f,  7.8094f}, {-20.3704f,  3.8088f}, {-18.7407f,  0.0481f},
      {-17.1111f, -2.8126f}, {-15.4815f, -4.5790f}, {-13.8519f, -5.4992f},
      {-12.2222f, -6.0600f}, {-10.5926f, -6.6956f}, { -8.9630f, -7.5404f},
      { -7.3333f, -8.3316f}, { -5.7037f, -8.5085f}, { -4.0741f, -7.4674f},
      { -2.4444f, -4.8647f}, { -0.8148f, -0.8394f}, {  0.8148f,  3.9464f},
      {  2.4444f,  8.4751f}, {  3.4741f, 11.6911f}, {  5.7037f, 12.8492f},
      {  7.9333f, 11.7603f}, {  8.9630f,  8.8384f}, { 10.5926f,  4.9343f},
      { 12.2222f,  1.0235f}, { 13.8519f, -2.1281f}, { 15.4815f, -4.1901f},
      { 17.1111f, -5.3018f}, { 18.7407f, -5.9113f}, { 20.3704f, -6.4965f},
      { 22.0000f, -7.2927f},
  };
}

// =============================================================================
// OFFSETS DE ARMA — anexação ao bone da mão
// =============================================================================
// Matrizes que rotacionam/escalam o modelo da arma para alinhar com o eixo do
// bone do esqueleto. Calculadas uma vez e guardadas em cada TroopTier.

glm::mat4 makeBowOffset() {
  // Linhas zeradas exceto onde indicado: troca eixos + flip + scale uniforme.
  glm::mat4 offset = glm::mat4(0.0f);
  const float s = 5.0f;
  offset[0][2] = s;
  offset[1][1] = s;
  offset[2][0] = -s;
  offset[3][3] = 1.0f;
  offset[3][1] = -7.0f;
  return offset;
}

glm::mat4 makeArquebusOffset() {
  const float s = 8.0f;
  Matrix<4, 4> mScale = scale<4, 4>(s, s, s);
  Matrix<4, 4> mRotY  = rotateY<4, 4>(-math_constants::kHalfPi);
  Matrix<4, 4> mRotX  = rotateX<4, 4>(-20.0f * math_constants::kDegToRad);
  Matrix<4, 4> mRotZ  = rotateZ<4, 4>(-math_constants::kHalfPi);
  Matrix<4, 4> mTrans = translate<4, 4>(0.0f, 0.0f, -4.0f);
  Matrix<4, 4> custom = mTrans * mRotY * mRotX * mScale * mRotZ;
  auto glOffset = toOpenGLMatrix(custom);
  return glm::make_mat4(glOffset.data());
}

// Empurra um tier "uniforme" (todas as variantes herdam texturas/arma) num def.
void addTier(TroopDef &def,
             AnimatedModel *model,
             unsigned int color,
             unsigned int normal,
             Mesh *weaponMesh,
             unsigned int weaponTexture,
             const glm::mat4 &weaponOffset) {
  TroopTier tier;
  tier.model = model;
  tier.texture = color;
  tier.normalMap = normal;
  tier.weaponMesh = weaponMesh;
  tier.weaponTexture = weaponTexture;
  tier.weaponOffset = weaponOffset;
  def.tiers.push_back(tier);
}

// =============================================================================
// LUZ DIRECIONAL — lua noturna (fria, azulada)
// =============================================================================
DirectionalLight makeMoonLight() {
  DirectionalLight l;
  l.direction = glm::normalize(glm::vec3(-0.4f, 1.2f, -0.5f));
  l.ambient   = glm::vec3(0.07f, 0.08f, 0.14f);
  l.diffuse   = glm::vec3(0.56f, 0.58f, 0.82f);
  l.specular  = glm::vec3(0.14f, 0.16f, 0.26f);
  return l;
}

// =============================================================================
// MAIN — toda a alocação GL acontece dentro de run() para garantir que os
// destrutores rodem com o contexto GL ainda válido.
// =============================================================================
int run(GLFWwindow *window) {
  using namespace render_constants;

  AppState state;
  glfwSetWindowUserPointer(window, &state);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  glfwGetFramebufferSize(window, &state.fbWidth, &state.fbHeight);
  glViewport(0, 0, state.fbWidth, state.fbHeight);

  glEnable(GL_DEPTH_TEST);

  registerInputCallbacks(window);

  // ---------------------------------------------------------------------------
  // HUD + texturas da HUD
  // ---------------------------------------------------------------------------
  Hud gameHud;
  gameHud.init(window);
  HudTextures uiTextures;
  uiTextures.topBackground = loadTexture("data/textures/ui_topbg.png", 4);
  uiTextures.goldIcon      = loadTexture("data/textures/ui_gold.png", 4);
  uiTextures.healthIcon    = loadTexture("data/textures/ui_health.png", 4);
  uiTextures.archerIcon    = loadTexture("data/textures/ui_archer.png", 4);
  uiTextures.arquebusIcon  = loadTexture("data/textures/ui_arquebus.png", 4);
  uiTextures.cannonIcon     = loadTexture("data/textures/ui_cannoner.png", 4);
  uiTextures.knightIcon     = loadTexture("data/textures/ui_knight.png", 4);
  uiTextures.zombiePortrait = loadTexture("data/textures/ui_zombie.png", 4);
  gameHud.setTextures(uiTextures);

  // ---------------------------------------------------------------------------
  // Shaders + uniforms
  // ---------------------------------------------------------------------------
  ShaderPipeline pipe;
  if (!loadAllShaders(pipe)) {
    deleteAllShaders(pipe);
    return 1;
  }
  setStaticUniforms(pipe);

  // ---------------------------------------------------------------------------
  // Modelos animados + estáticos
  // ---------------------------------------------------------------------------
  AnimatedModel enemyBase("data/models/zombie/zombie_t.glb");
  enemyBase.loadAnimation("run",   "data/models/zombie/zombie_run.glb");
  enemyBase.loadAnimation("death", "data/models/zombie/zombie_death.glb");

  AnimatedModel armoredZombieBase("data/models/armoredzombie/armoredzombie.glb");
  armoredZombieBase.loadAnimation("run",   "data/models/armoredzombie/armoredzombie_walk.glb");
  armoredZombieBase.loadAnimation("death", "data/models/armoredzombie/armoredzombie_death.glb");

  // Arcabuz — 5 tiers (sistema de upgrade)
  AnimatedModel arquebusBase("data/models/arquebus/arquebus_t.glb");
  arquebusBase.loadAnimation("idle1", "data/models/arquebus/arquebus_idle.glb");
  AnimatedModel arquebusBaseLvl2("data/models/arquebus/arquebus_t2.glb");
  arquebusBaseLvl2.loadAnimation("idle1", "data/models/arquebus/arquebus_idle.glb");
  AnimatedModel arquebusBaseLvl3("data/models/arquebus/arquebus_t3.glb");
  arquebusBaseLvl3.loadAnimation("idle1", "data/models/arquebus/arquebus_idle.glb");
  AnimatedModel arquebusBaseLvl4("data/models/arquebus/arquebus_t4.glb");
  arquebusBaseLvl4.loadAnimation("idle1", "data/models/arquebus/arquebus_idle.glb");
  AnimatedModel arquebusBaseLvl5("data/models/arquebus/arquebus_t5.glb");
  arquebusBaseLvl5.loadAnimation("idle1", "data/models/arquebus/arquebus_idle.glb");


  for (AnimatedModel *m : {&arquebusBase, &arquebusBaseLvl2, &arquebusBaseLvl3,
                            &arquebusBaseLvl4, &arquebusBaseLvl5}) {
    m->loadAnimation("fire",   "data/models/arquebus/arquebus_fire.glb");
    m->loadAnimation("reload", "data/models/arquebus/arquebus_reload.glb");
  }

  // Arqueiro — 5 tiers (sistema de upgrade)
  AnimatedModel archerBase("data/models/archer/archer_t.glb");
  archerBase.loadAnimation("idle1", "data/models/archer/idle1.glb");
  archerBase.loadAnimation("idle2", "data/models/archer/idle2.glb");
  archerBase.loadAnimation("idle3", "data/models/archer/idle3.glb");
  archerBase.loadAnimation("idle4", "data/models/archer/idle4.glb");
  archerBase.loadAnimation("aim", "data/models/archer/aim_draw.glb");
  AnimatedModel archerBaseLvl2("data/models/archer/archer_t2.glb");
  archerBaseLvl2.loadAnimation("idle1", "data/models/archer/idle1.glb");
  archerBaseLvl2.loadAnimation("idle2", "data/models/archer/idle2.glb");
  archerBaseLvl2.loadAnimation("aim", "data/models/archer/aim_draw.glb");
  AnimatedModel archerBaseLvl3("data/models/archer/archer_t3.glb");
  archerBaseLvl3.loadAnimation("idle1", "data/models/archer/idle1.glb");
  archerBaseLvl3.loadAnimation("aim", "data/models/archer/aim_draw.glb");
  AnimatedModel archerBaseLvl4("data/models/archer/archer_t4.glb");
  archerBaseLvl4.loadAnimation("idle1", "data/models/archer/idle1.glb");
  archerBaseLvl4.loadAnimation("aim", "data/models/archer/aim_draw.glb");
  AnimatedModel archerBaseLvl5("data/models/archer/archer_t5.glb");
  archerBaseLvl5.loadAnimation("idle1", "data/models/archer/idle1.glb");
  archerBaseLvl5.loadAnimation("aim", "data/models/archer/aim_draw.glb");

  // Canhoneiro — tier único (sem modelos por nível ainda)
  AnimatedModel cannonerBase("data/models/cannon/cannoner_t.glb");
  cannonerBase.loadAnimation("idle1", "data/models/cannon/cannoner_idle.glb");
  cannonerBase.loadAnimation("fire",  "data/models/cannon/cannoner_fire.glb");
  cannonerBase.loadAnimation("cower", "data/models/cannon/cannoner_cower.glb");

  AnimatedModel knightBase("data/models/knight/knight_t.glb");
  knightBase.loadAnimation("knightWalk",  "data/models/knight/knight_walk.glb");
  knightBase.loadAnimation("knightIdle",  "data/models/knight/knight_idle.glb");
  knightBase.loadAnimation("knightSlash", "data/models/knight/knight_attack.glb");
  knightBase.loadAnimation("knightDeath", "data/models/zombie/zombie_death.glb");
  knightBase.loadAnimation("command",     "data/models/knight/knight_command.glb");


  std::vector<Vertex> bowVertices;
  if (!loadObj("data/models/archer/bow.obj", bowVertices)) {
    std::cout << "ERRO: Nao encontrou data/models/archer/bow.obj" << std::endl;
  }
  Mesh bowMesh(bowVertices);

  std::vector<Vertex> swordVertices;
if (!loadObj("data/models/knight/sword.obj", swordVertices)) {
  std::cout << "ERRO: Nao encontrou sword.obj" << std::endl;
}
Mesh swordMesh(swordVertices);

std::vector<Vertex> shieldVertices;
if (!loadObj("data/models/knight/shield.obj", shieldVertices)) {
  std::cout << "ERRO: Nao encontrou shield.obj" << std::endl;
}
Mesh shieldMesh(shieldVertices);
  // Quad plano XZ para o círculo de seleção (centrado na origem, escala via matrix)
  std::vector<Vertex> circleQuadVerts = {
      {{-1.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
      {{ 1.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
      {{ 1.0f, 0.0f,  1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
      {{-1.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
      {{ 1.0f, 0.0f,  1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
      {{-1.0f, 0.0f,  1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
  };
  Mesh selectionCircleMesh(circleQuadVerts);

  // Circulo de range: 64 segmentos no plano XZ, raio=1 (escalar por range no draw)
  GLuint rangeCircleVAO = 0, rangeCircleVBO = 0;
  {
    const int kSegs = 64;
    std::vector<float> pts;
    pts.reserve(kSegs * 3);
    for (int i = 0; i < kSegs; ++i) {
      float a = 2.0f * math_constants::kPi * i / kSegs;
      pts.push_back(std::cos(a));
      pts.push_back(0.05f);
      pts.push_back(std::sin(a));
    }
    glGenVertexArrays(1, &rangeCircleVAO);
    glGenBuffers(1, &rangeCircleVBO);
    glBindVertexArray(rangeCircleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, rangeCircleVBO);
    glBufferData(GL_ARRAY_BUFFER, pts.size() * sizeof(float), pts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
  }

  std::vector<Vertex> arquebusVertices;
  if (!loadObj("data/models/arquebus/arquebus_weapon.obj", arquebusVertices)) {
    std::cout << "ERRO: Nao encontrou arquebus_weapon.obj" << std::endl;
  }
  Mesh arquebusMesh(arquebusVertices);

  std::vector<Vertex> cannonBarrelVertices;
  if (!loadObj("data/models/cannon/cannon.obj", cannonBarrelVertices)) {
    std::cout << "ERRO: Nao encontrou cannon.obj" << std::endl;
  }
  Mesh cannonBarrelMesh(cannonBarrelVertices);

  std::vector<Vertex> torchVertices;
  if (!loadObj("data/models/cannon/torch.obj", torchVertices)) {
    std::cout << "ERRO: Nao encontrou torch.obj" << std::endl;
  }
  Mesh torchMesh(torchVertices);

  std::vector<Vertex> castleVertices;
  loadObj("data/models/world/castle.obj", castleVertices);
  Mesh castleMesh(castleVertices);

  // Árvore: tenta carregar leaves+log separados; senão, fallback para tree.obj
  std::vector<Vertex> treeLeavesVertices, treeLogVertices;
  std::unique_ptr<Mesh> treeLeavesMesh;
  std::unique_ptr<Mesh> treeLogMesh;
  if (loadObj("data/models/world/leaves.obj", treeLeavesVertices) && !treeLeavesVertices.empty()) {
    treeLeavesMesh.reset(new Mesh(treeLeavesVertices));
  } else {
    std::cout << "[trees] sem leaves.obj utilizável" << std::endl;
  }
  if (loadObj("data/models/world/log.obj", treeLogVertices) && !treeLogVertices.empty()) {
    treeLogMesh.reset(new Mesh(treeLogVertices));
  } else {
    std::cout << "[trees] sem log.obj utilizável" << std::endl;
  }
  if (!treeLeavesMesh && !treeLogMesh) {
    if (loadObj("data/models/world/tree.obj", treeLogVertices) && !treeLogVertices.empty()) {
      treeLogMesh.reset(new Mesh(treeLogVertices));
      std::cout << "[trees] Fallback: carregou data/models/world/tree.obj" << std::endl;
    } else {
      std::cout << "[trees] nenhum mesh de arvore carregado; nao vai desenhar." << std::endl;
    }
  }

  std::vector<Vertex> lanternVertices;
  std::unique_ptr<Mesh> lanternMesh;
  if (loadObj("data/models/world/lantern.obj", lanternVertices) && !lanternVertices.empty()) {
    lanternMesh.reset(new Mesh(lanternVertices));
  } else {
    std::cout << "ERRO: lantern.obj nao encontrado/invalido" << std::endl;
  }

  // ---------------------------------------------------------------------------
  // Texturas da cena
  // ---------------------------------------------------------------------------
  SceneTextures tex = loadAllSceneTextures();

  // ---------------------------------------------------------------------------
  // Geometria primitiva + path + placement procedural
  // ---------------------------------------------------------------------------
  GpuMesh grassMesh = createGrassMesh();
  GpuMesh skyMesh = createSkyboxMesh();

  std::vector<Point> controlPoints = defaultControlPoints();
  std::vector<Point> curvePoints = generateCatmullRomVertices(controlPoints);
  GpuMesh curveMesh = createCurveMesh(curvePoints);
  PathCache curveCache = buildPathCache(curvePoints);
  Mesh pathMesh = generatePathMesh(curvePoints, 2.0f);

  std::vector<TreeInstance> trees = placeTrees(curvePoints);

  std::vector<PointLight> lanternLights;
  std::vector<LanternInstance> lanterns = placeLanterns(curvePoints, curveCache, lanternLights);

  // ---------------------------------------------------------------------------
  // Catálogos de tropa (TroopDefs) — referenciam modelos/texturas/armas já
  // carregados. Cada def vira a fonte de verdade para placement e upgrade.
  // ---------------------------------------------------------------------------
  const glm::mat4 bowOffset = makeBowOffset();
  const glm::mat4 arqOffset = makeArquebusOffset();

  TroopDef enemyClass;
  enemyClass.type = 3;
  addTier(enemyClass, &enemyBase, tex.enemyColor, 0, nullptr, 0, glm::mat4(1.0f));

  TroopDef armoredEnemyClass;
  armoredEnemyClass.type = 3;
  addTier(armoredEnemyClass, &armoredZombieBase, tex.armoredZombieColor, 0, nullptr, 0, glm::mat4(1.0f));

  TroopDef archerClass;
  archerClass.type = 1;
  for (AnimatedModel *m : {&archerBase, &archerBaseLvl2, &archerBaseLvl3,
                            &archerBaseLvl4, &archerBaseLvl5}) {
    addTier(archerClass, m, tex.archerColor, tex.archerNormal,
            &bowMesh, tex.bowColor, bowOffset);
  }

  TroopDef arquebusClass;
  arquebusClass.type = 2;
  for (AnimatedModel *m : {&arquebusBase, &arquebusBaseLvl2, &arquebusBaseLvl3,
                            &arquebusBaseLvl4, &arquebusBaseLvl5}) {
    addTier(arquebusClass, m, tex.arquebusColor, tex.arquebusNormal,
            &arquebusMesh, tex.arquebusWeapon, arqOffset);
  }

  TroopDef cannonClass;
  cannonClass.type = defender_types::kCannon;
  addTier(cannonClass, &cannonerBase, tex.cannonerColor, tex.cannonerNormal,
          nullptr, 0, glm::mat4(1.0f));

  TroopDef knightClass;
  knightClass.type = defender_types::kKnight;
  addTier(knightClass, &knightBase, tex.knightColor, tex.knightNormal,
  nullptr, 0, glm::mat4(1.0f));

  // ---------------------------------------------------------------------------
  // Estado de combate + iluminação
  // ---------------------------------------------------------------------------
  constexpr int kMaxEnemies = 20;

  // Dois pools de GameObjects — um por tipo de inimigo.
  // Cada GO tem estado de animação independente; compartilham o AnimatedModel.
  std::vector<GameObject> normalEnemyModels, armoredEnemyModels;
  normalEnemyModels.reserve(kMaxEnemies);
  armoredEnemyModels.reserve(kMaxEnemies);
  for (int i = 0; i < kMaxEnemies; ++i) {
    normalEnemyModels.emplace_back(&enemyClass, Vector<3>{0.f, 0.f, 0.f});
    normalEnemyModels.back().setIdleAnimations({"run"});
    armoredEnemyModels.emplace_back(&armoredEnemyClass, Vector<3>{0.f, 0.f, 0.f});
    armoredEnemyModels.back().setIdleAnimations({"run"});
  }

  // Todos os slots começam mortos e controlados pela wave.
  EnemyStats defaultStats = kZombieStats;
  std::vector<EnemyInstance> enemies(kMaxEnemies);
  for (auto &e : enemies) {
    e = makeEnemy(defaultStats);
    e.waveControlled = true;
    e.alive          = false;
  }

  std::vector<EnemyTickResult> enemyTicks(kMaxEnemies);

  std::vector<GameObject> defenders;
  std::vector<DefenderShoot> defenderShoots;
  CannonSmoke cannonSmoke;

  std::vector<KnightInstance> walkingKnights;
  std::vector<GameObject>     walkingKnightModels;

  KnightWeaponTweaks weaponTweaks;

  int selectedTroopIndex = -1;
  float selectionAngle = 0.0f;
  bool battleMusicStarted = false;

  // Wave system — controla spawn
  WaveState waveState = makeWaveState();

  DirectionalLight moonLight = makeMoonLight();

  // ---------------------------------------------------------------------------
  // Spell mode (desenho + CNN) — carrega pesos uma vez.
  // ---------------------------------------------------------------------------
  spell::ShapeClassifier spellClassifier;
  spellClassifier.load("data/shape_classifier_weights.bin");
  spell::SpellMode spellMode;
  spell::init(spellMode);

  Camera cam;
  Vector<3> cameraPosition{0.0f, 2.0f, 5.0f};
  Matrix<4, 4> identity = Matrix<4, 4>::identity();

  // ---------------------------------------------------------------------------
  // LOOP PRINCIPAL
  // ---------------------------------------------------------------------------
  // Trilhas disponíveis para seleção aleatória
  static const char* kBattleTracks[] = {
    "data/audio/battle1.mp3", "data/audio/battle2.mp3", "data/audio/battle3.mp3"
  };
  static const char* kIntermissionTracks[] = {
    "data/audio/intermission1.mp3", "data/audio/intermission2.mp3", "data/audio/intermission3.mp3"
  };
  static const char* kArcherShots[]   = {"data/audio/archer_shot1.mp3",   "data/audio/archer_shot2.mp3"};
  static const char* kArquebusShots[] = {"data/audio/arquebus_shot1.mp3", "data/audio/arquebus_shot2.mp3"};

  // Música de intermission começa imediatamente; posição preservada entre ondas
  audio::startIntermissionMusic(kIntermissionTracks[std::rand() % 3]);

  using namespace game_constants;
  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    double currentTime = glfwGetTime();
    if (currentTime - lastTime < kFrameDelay) {
      continue;
    }
    const float deltaTime = static_cast<float>(kFrameDelay);
    lastTime += kFrameDelay;

    selectionAngle += 0.5f * deltaTime;
    if (selectionAngle >= math_constants::kTwoPi) selectionAngle -= math_constants::kTwoPi;

    // ------- UPDATE: inimigos -------
    int deathsThisFrame = 0;
    for (auto &t : enemyTicks) { t.position = {0.f, -1000.f, 0.f}; t.diedThisFrame = false; }
    for (int i = 0; i < kMaxEnemies; ++i) {
      auto &e = enemies[i];
      if (!e.alive && e.respawnTimer <= 0.0f) continue;
      GameObject &model = (e.stats.type == enemy_types::kArmored)
                          ? armoredEnemyModels[i] : normalEnemyModels[i];
      enemyTicks[i] = updateEnemy(e, model, curvePoints, curveCache, state, deltaTime);
      if (enemyTicks[i].diedThisFrame) ++deathsThisFrame;
    }

    // ------- UPDATE: wave system -------
    bool allSlotsFull = true;
    for (auto &e : enemies)
      if (!e.alive && e.respawnTimer <= 0.0f) { allSlotsFull = false; break; }

    WavePhase prevPhase = waveState.phase;
    int spawnType = updateWave(waveState, window, deltaTime, deathsThisFrame, allSlotsFull);
    if (spawnType >= 0) {
      const WaveDef &waveDef = kWaves[waveState.currentWave];
      const EnemyStats &stats = (spawnType == enemy_types::kArmored)
                                ? waveDef.armoredStats : waveDef.enemyStats;
      for (int i = 0; i < kMaxEnemies; ++i) {
        if (!enemies[i].alive && enemies[i].respawnTimer <= 0.0f) {
          enemies[i].stats        = stats;
          enemies[i].hp           = stats.maxHp;
          enemies[i].pathDistance = 0.0f;
          enemies[i].alive        = true;
          enemies[i].hitFlashTime = 0.0f;
          enemies[i].respawnTimer = 0.0f;
          clearSpellEffects(enemies[i]);
          GameObject &model = (spawnType == enemy_types::kArmored)
                              ? armoredEnemyModels[i] : normalEnemyModels[i];
          model.setAnimation("run");
          break;
        }
      }
    }

    // --- Transições de fase: troca de música e efeitos ---
    if (waveState.phase != prevPhase) {
      if (waveState.phase == WavePhase::Starting) {
        audio::pauseIntermissionMusic();
        audio::playOneShotAt("data/audio/wave_start.mp3", 0.25f);
      } else if (waveState.phase == WavePhase::Active) {
        if (battleMusicStarted) {
          audio::resumeBattleMusic();
        } else {
          audio::playMusic(kBattleTracks[std::rand() % 3]);
          battleMusicStarted = true;
        }
      } else if (waveState.phase == WavePhase::Intermission) {
        audio::pauseBattleMusic();
        audio::resumeIntermissionMusic();
      } else if (waveState.phase == WavePhase::Victory) {
        audio::stopMusic();
        audio::stopIntermissionMusic();
      }
    }

    // ------- UPDATE: defensores -------
    DefenderFireResult fireResult = updateDefenders(
        defenders, defenderShoots, enemies, enemyTicks, deltaTime);
    for (int n = 0; n < fireResult.archerFired;   ++n) audio::playOneShot(kArcherShots[std::rand() % 2]);
    for (int n = 0; n < fireResult.arquebusFired; ++n) audio::playOneShot(kArquebusShots[std::rand() % 2]);
    for (int n = 0; n < fireResult.cannonFired;   ++n) audio::playOneShot("data/audio/cannon1.mp3");
    for (const auto &pos : fireResult.cannonShotPositions)  cannonSmoke.emit(pos, 10, 1.0f);
    for (const auto &pos : fireResult.arquebusShotPositions) cannonSmoke.emit(pos, 4, 0.35f);
    cannonSmoke.update(deltaTime);

    // Invocações dos comandantes de cavaleiros
    for (int n = 0; n < fireResult.knightSummoned; ++n) {
      walkingKnights.push_back(makeKnight(curveCache));
      walkingKnightModels.emplace_back(&knightClass, Vector<3>{0.0f, 0.0f, 0.0f});
      walkingKnightModels.back().setIdleAnimations({"knightWalk"});
      walkingKnightModels.back().setAnimation("knightWalk");
    }
    static const char *kChargeShots[] = {
        "data/audio/charge1.mp3", "data/audio/charge2.mp3", "data/audio/charge3.mp3",
        "data/audio/charge4.mp3", "data/audio/charge5.mp3", "data/audio/charge6.mp3",
    };
    for (int n = 0; n < fireResult.chargePlayed; ++n)
      audio::playOneShot(kChargeShots[std::rand() % 6]);

    // Atualiza cavaleiros em campo
    std::vector<KnightTickResult> walkingKnightTicks;
    for (int i = 0; i < (int)walkingKnights.size(); ) {
      if (!walkingKnights[i].alive && !walkingKnights[i].dying) {
        walkingKnights.erase(walkingKnights.begin() + i);
        walkingKnightModels.erase(walkingKnightModels.begin() + i);
        continue;
      }
      KnightTickResult tick = updateKnight(
          walkingKnights[i], walkingKnightModels[i],
          enemies, enemyTicks, curvePoints, curveCache, deltaTime);
      if (tick.hitThisFrame) audio::playOneShot("data/audio/knight_hit.mp3");
      walkingKnightTicks.push_back(tick);
      ++i;
    }
    // ------- INPUT por frame -------
    processInput(window, cameraPosition, deltaTime);
    spell::update(spellMode, state, window, spellClassifier, deltaTime);

    // ------- Clear + camera setup -------
    glClearColor(kFogColor.r, kFogColor.g, kFogColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float aspect = static_cast<float>(state.fbWidth) / static_cast<float>(state.fbHeight);
    cam.setPerspective(kFovDegrees * math_constants::kDegToRad, aspect, kNearPlane, kFarPlane);

    // Posição usada para render/picking neste frame. `cameraPosition` guarda o
    // estado PERSISTENTE da câmera livre; aéreo/orbital derivam uma posição
    // própria sem sobrescrevê-lo, para a livre voltar de onde parou.
    Vector<3> renderCameraPos = cameraPosition;
    if (state.cameraMode == CameraMode::Free) {
      cam.setUp(Vector<3>{0.0f, 1.0f, 0.0f});
      cam.setFPS(cameraPosition, state.yaw, state.pitch);
    } else if (state.cameraMode == CameraMode::Orbital) {
      cam.setUp(Vector<3>{0.0f, 1.0f, 0.0f});
      updateOrbitalCameraPosition(state, renderCameraPos);
      cam.setLookAt(renderCameraPos, state.orbitTarget);
    } else {
      // Aerial: alto, olhando direto para baixo; pan no plano XZ (WASD).
      // up = (0,0,-1): eixo -Z aponta para cima na tela (norte do mapa).
      renderCameraPos = Vector<3>{state.aerialPanX, kAerialCameraHeight, state.aerialPanZ};
      cam.setUp(Vector<3>{0.0f, 0.0f, -1.0f});
      cam.setLookAt(renderCameraPos, Vector<3>{state.aerialPanX, 0.0f, state.aerialPanZ});
    }

    auto glView = toOpenGLMatrix(cam.getViewMatrix());
    auto glProj = toOpenGLMatrix(cam.getProjectionMatrix());
    const glm::vec3 glmViewPos(renderCameraPos[0], renderCameraPos[1], renderCameraPos[2]);

    // ------- FEITIÇOS: aplica o efeito das conjurações novas aos inimigos -------
    // A forma é desenhada/classificada em espaço de tela; aqui projetamos cada
    // inimigo vivo para a tela e testamos se cai dentro da forma perfeita. Cada
    // conjuração é aplicada uma única vez (flag applied).
    for (auto &cast : spellMode.casts) {
      if (cast.applied) continue;
      cast.applied = true;
      for (int i = 0; i < kMaxEnemies; ++i) {
        if (!enemies[i].alive) continue;
        const Vector<3> &wp = enemyTicks[i].position;
        // worldToScreen (column-major, igual ao usado nos tooltips abaixo).
        float wx = wp[0], wy = 1.0f, wz = wp[2];  // y=1: meio do corpo
        float vx = glView[0]*wx + glView[4]*wy + glView[8] *wz + glView[12];
        float vy = glView[1]*wx + glView[5]*wy + glView[9] *wz + glView[13];
        float vz = glView[2]*wx + glView[6]*wy + glView[10]*wz + glView[14];
        float vw = glView[3]*wx + glView[7]*wy + glView[11]*wz + glView[15];
        float cx = glProj[0]*vx + glProj[4]*vy + glProj[8] *vz + glProj[12]*vw;
        float cy = glProj[1]*vx + glProj[5]*vy + glProj[9] *vz + glProj[13]*vw;
        float cw = glProj[3]*vx + glProj[7]*vy + glProj[11]*vz + glProj[15]*vw;
        if (cw <= 0.0f) continue;  // atrás da câmera
        float sx = ( cx / cw + 1.0f) * 0.5f * static_cast<float>(state.fbWidth);
        float sy = (-cy / cw + 1.0f) * 0.5f * static_cast<float>(state.fbHeight);
        if (!spell::pointInCast(cast, sx, sy)) continue;

        // Despacha por forma: 0=círculo (veneno), 1=quadrado (lentidão),
        // 2=triângulo (metade da vida).
        if (cast.shape == 0)      spell_effects::applyPoison(enemies[i]);
        else if (cast.shape == 1) spell_effects::applySlow(enemies[i]);
        else if (cast.shape == 2) spell_effects::applyHalfLife(enemies[i]);
      }
    }

    // Constrói lista de luzes combinada (lanternas + tochas dos canhoneiros).
    std::vector<PointLight> allLights = lanternLights;
    for (const auto &unit : defenders) {
      if (unit.type != defender_types::kCannon) continue;
      if (allLights.size() >= static_cast<size_t>(kMaxPointLights)) break;
      const float bkX = std::sin(unit.rotationY);
      const float bkZ = -std::cos(unit.rotationY);
      PointLight tl;
      tl.position = glm::vec3(unit.position[0] + bkX * kCannonerBehindOffset,
                              unit.position[1] + kLanternLightHeight,
                              unit.position[2] + bkZ * kCannonerBehindOffset);
      tl.color = kLanternLightColor;
      allLights.push_back(tl);
    }

    // ------- RENDER: entidades animadas (inimigo + defensores) -------
    uploadCommonUniforms(pipe.animShader, moonLight, allLights, glmViewPos, glView, glProj);
    glUniform1f(glGetUniformLocation(pipe.animShader, "hitFlash"), 0.0f);
    for (int i = 0; i < kMaxEnemies; ++i) {
      auto &e = enemies[i];
      if (!e.alive && e.respawnTimer <= 0.0f) continue;
      bool isArmored = (e.stats.type == enemy_types::kArmored);
      GameObject &model = isArmored ? armoredEnemyModels[i] : normalEnemyModels[i];
      unsigned int eTex = isArmored ? tex.armoredZombieColor : tex.enemyColor;
      renderEnemy(pipe.animShader, e, model, enemyTicks[i].position, enemyTicks[i].angle,
                  eTex, tex.defaultNormal);
    }

    for (int i = 0; i < (int)walkingKnights.size(); ++i) {
      if (!walkingKnights[i].alive && !walkingKnights[i].dying) continue;
      renderKnight(pipe.animShader, walkingKnightModels[i],
                   walkingKnightTicks[i].position, walkingKnightTicks[i].angle,
                   tex.knightColor, tex.knightNormal);
    }

    renderDefenders(pipe.animShader, defenders, tex.archerColor, tex.archerNormal);

    // ------- RENDER: armas dos defensores + castelo -------
    uploadCommonUniforms(pipe.objShader, moonLight, allLights, glmViewPos, glView, glProj);
    renderDefenderWeapons(pipe.objShader, pipe.objU, defenders,
                          bowMesh, tex.bowColor, torchMesh, tex.torchColor);
    renderCannonBarrels(pipe.objShader, pipe.objU, defenders, cannonBarrelMesh, tex.cannonBarrelColor);
    for (int i = 0; i < (int)walkingKnights.size(); ++i) {
      if (!walkingKnights[i].alive && !walkingKnights[i].dying) continue;
      renderKnightWeapons(pipe.objShader, pipe.objU, walkingKnightModels[i],
                          swordMesh, tex.swordColor,
                          shieldMesh, tex.shieldColor,
                          weaponTweaks);
    }
    renderCastle(pipe.objShader, pipe.objU, castleMesh, tex.castleColor, tex.castleNormal,
                 curvePoints);

    // ------- RENDER: chão de grama -------
    uploadCommonUniforms(pipe.groundShader, moonLight, allLights, glmViewPos, glView, glProj);
    renderGround(grassMesh, tex.grass, tex.noise);

    // ------- RENDER: árvores -------
    uploadCommonUniforms(pipe.objShader, moonLight, allLights, glmViewPos, glView, glProj);
    renderTrees(pipe.objShader, pipe.objU, trees,
                treeLogMesh.get(), tex.treeLog,
                treeLeavesMesh.get(), tex.treeLeaves);

    // ------- RENDER: círculo de range (azul) -------
    if (selectedTroopIndex >= 0 && selectedTroopIndex < static_cast<int>(defenders.size()) &&
        defenders[selectedTroopIndex].type != defender_types::kKnight) {
      const GameObject &sel = defenders[selectedTroopIndex];
      float r = sel.range;
      auto glRangeModel = toOpenGLMatrix(
          translate<4,4>(sel.position[0], 0.0f, sel.position[2]) * scale<4,4>(r, 1.0f, r));

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glLineWidth(2.0f);
      glUseProgram(pipe.lineShader);
      glUniformMatrix4fv(pipe.lineU.view,       1, GL_FALSE, glView.data());
      glUniformMatrix4fv(pipe.lineU.projection, 1, GL_FALSE, glProj.data());
      glUniformMatrix4fv(pipe.lineU.model,      1, GL_FALSE, glRangeModel.data());
      glUniform4f(pipe.lineU.color, 0.25f, 0.55f, 1.0f, 0.85f);
      glBindVertexArray(rangeCircleVAO);
      glDrawArrays(GL_LINE_LOOP, 0, 64);
      glBindVertexArray(0);
      glLineWidth(1.0f);
      glDisable(GL_BLEND);
    }

    // ------- RENDER: círculo de seleção -------
    if (selectedTroopIndex >= 0 && selectedTroopIndex < static_cast<int>(defenders.size())) {
      const GameObject &sel = defenders[selectedTroopIndex];
      const float px = sel.position[0], pz = sel.position[2];
      Matrix<4,4> mT = translate<4,4>(px, 0.01f, pz);
      Matrix<4,4> mR = rotateY<4,4>(selectionAngle);
      Matrix<4,4> mS = scale<4,4>(0.8f, 1.0f, 0.8f);
      auto glCircleModel = toOpenGLMatrix(mT * mR * mS);

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glDepthMask(GL_FALSE);

      glUseProgram(pipe.objShader);
      glUniformMatrix4fv(pipe.objU.model, 1, GL_FALSE, glCircleModel.data());
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, tex.selectionCircle);
      selectionCircleMesh.draw();

      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
    }

    // ------- RENDER: lanternas -------
    if (lanternMesh) {
      uploadCommonUniforms(pipe.lanternShader, moonLight, allLights, glmViewPos, glView, glProj);
      renderLanterns(pipe.lanternU, lanterns, *lanternMesh, tex.lantern);
    }

    // Trecho herdado: forçar clamp do sky texture antes do path (preservado para
    // não mudar comportamento de wrap noutros draws subsequentes).
    glBindTexture(GL_TEXTURE_2D, tex.sky);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // ------- RENDER: caminho de terra -------
    uploadCommonUniforms(pipe.pathShader, moonLight, lanternLights, glmViewPos, glView, glProj);
    renderPath(pipe.pathU, pathMesh, tex.dirt, tex.noise, identity);

    // ------- RENDER: céu noturno -------
    renderSky(pipe.skyShader, pipe.skyViewLoc, pipe.skyProjLoc, pipe.skyTexLoc,
              skyMesh, tex.sky, cam, glProj);

    // ------- RENDER: curva de debug (toggle T) -------
    if (state.showCurve) {
      renderCurveOverlay(pipe.lineShader, pipe.lineU, curveMesh, glView, glProj);
    }

    // ------- RENDER: preview de posicionamento de tropa -------
    if (state.isPlacingTroop && !state.isDrawingSpell) {
      TroopPlacementContext ctx{
          window, cam, renderCameraPos, curvePoints, pipe.previewShader,
          archerClass, arquebusClass, cannonClass, knightClass,
          defenders, defenderShoots,
          glView.data(), glProj.data(),
      };
      handleTroopPlacement(ctx, state, deltaTime);
    }

    // ------- SELEÇÃO DE TROPA (clique no mundo) -------
    selectedTroopIndex = troop_selection::update(
        window, cam, renderCameraPos, defenders, state, selectedTroopIndex);

    // ------- RENDER: fumaça dos canhões -------
    {
      const glm::vec3 camRight(glView[0], glView[4], glView[8]);
      const glm::vec3 camUp   (glView[1], glView[5], glView[9]);
      cannonSmoke.render(pipe.particleShader, tex.smokeTexture, camRight, camUp, glView, glProj);
    }

    // ------- HUD + janela de upgrade (ImGui frame manual aqui pra empilhar) -------
    const float currentFps = 1.0f / deltaTime;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    gameHud.render(state, waveState, currentFps);

    // Canvas overlay + janela de resultado da classificação (se houver).
    spell::render(spellMode, state);

    if (selectedTroopIndex >= 0 && selectedTroopIndex < static_cast<int>(defenders.size())) {
      GameObject &troop = defenders[selectedTroopIndex];
      if (drawTroopDetailsHud(troop, state, uiTextures.archerIcon)) {
        troop.upgrade();
        audio::playOneShot("data/audio/selection_sound.mp3");
      }
    }

    // ------- TOOLTIPS de unidades -------
    {
      ImGuiIO &io = ImGui::GetIO();
      if (!io.WantCaptureMouse) {
        ImVec2 mouse = ImGui::GetMousePos();

        // Projeta um ponto 3D → pixel de tela (column-major, igual ao shader).
        auto worldToScreen = [&](float wx, float wy, float wz) -> std::pair<ImVec2, bool> {
          float vx = glView[0]*wx + glView[4]*wy + glView[8] *wz + glView[12];
          float vy = glView[1]*wx + glView[5]*wy + glView[9] *wz + glView[13];
          float vz = glView[2]*wx + glView[6]*wy + glView[10]*wz + glView[14];
          float vw = glView[3]*wx + glView[7]*wy + glView[11]*wz + glView[15];
          float cx = glProj[0]*vx + glProj[4]*vy + glProj[8] *vz + glProj[12]*vw;
          float cy = glProj[1]*vx + glProj[5]*vy + glProj[9] *vz + glProj[13]*vw;
          float cw = glProj[3]*vx + glProj[7]*vy + glProj[11]*vz + glProj[15]*vw;
          if (cw <= 0.0f) return {{0.f, 0.f}, false};
          float sx = ( cx / cw + 1.0f) * 0.5f * static_cast<float>(state.fbWidth);
          float sy = (-cy / cw + 1.0f) * 0.5f * static_cast<float>(state.fbHeight);
          return {{sx, sy}, true};
        };

        // Projeta pés (Y=0) e cabeça (Y=kH) e verifica se o mouse está dentro
        // do retângulo de tela com padding horizontal kPad.
        const float kH   = 1.7f;   // altura aproximada do personagem em unidades do mundo
        const float kPad = 25.0f;  // padding horizontal em pixels
        auto overModel = [&](float wx, float wz) -> bool {
          auto [sFoot, okF] = worldToScreen(wx, 0.0f, wz);
          auto [sHead, okH] = worldToScreen(wx, kH,   wz);
          if (!okF || !okH) return false;
          float left  = std::min(sFoot.x, sHead.x) - kPad;
          float right = std::max(sFoot.x, sHead.x) + kPad;
          float top   = std::min(sFoot.y, sHead.y);
          float bot   = std::max(sFoot.y, sHead.y);
          return mouse.x >= left && mouse.x <= right
              && mouse.y >= top  && mouse.y <= bot;
        };

        bool showed = false;

        // Defensores colocados no mapa
        for (const auto &def : defenders) {
          if (!overModel(def.position[0], def.position[2])) continue;
          const char *nome = (def.type == defender_types::kArcher)   ? "Arqueiro"
                           : (def.type == defender_types::kCannon)   ? "Canhoneiro"
                           : (def.type == defender_types::kKnight)   ? "Comandante"
                                                                     : "Arcabuzeiro";
          ImGui::BeginTooltip();
          ImGui::Text("%s (N\xc3\xadvel %d)", nome, def.level);
          ImGui::EndTooltip();
          showed = true;
          break;
        }

        // Cavaleiros em campo (invocados)
        for (int i = 0; i < (int)walkingKnights.size() && !showed; ++i) {
          if (!walkingKnights[i].alive && !walkingKnights[i].dying) continue;
          if (!overModel(walkingKnightTicks[i].position[0], walkingKnightTicks[i].position[2])) continue;
          ImGui::BeginTooltip();
          ImGui::Text("Cavaleiro");
          ImGui::Separator();
          ImGui::Text("Vida: %d / %d", walkingKnights[i].hp, walkingKnights[i].maxHp);
          ImGui::EndTooltip();
          showed = true;
        }

        // Inimigos
        for (int i = 0; i < kMaxEnemies && !showed; ++i) {
          if (!enemies[i].alive) continue;
          if (!overModel(enemyTicks[i].position[0], enemyTicks[i].position[2])) continue;
          const char *nome = (enemies[i].stats.type == enemy_types::kArmored)
                             ? "Zumbi Blindado" : "Zumbi";
          ImGui::BeginTooltip();
          ImGui::Text("%s  %d/%d", nome, enemies[i].hp, enemies[i].stats.maxHp);
          ImGui::EndTooltip();
          showed = true;
        }
      }
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // ---------------------------------------------------------------------------
  // CLEANUP
  // ---------------------------------------------------------------------------
  gameHud.shutdown();
  deleteAllSceneTextures(tex);
  glDeleteTextures(1, &uiTextures.topBackground);
  glDeleteTextures(1, &uiTextures.goldIcon);
  glDeleteTextures(1, &uiTextures.healthIcon);
  glDeleteTextures(1, &uiTextures.archerIcon);
  glDeleteTextures(1, &uiTextures.arquebusIcon);
  glDeleteTextures(1, &uiTextures.cannonIcon);
  glDeleteTextures(1, &uiTextures.knightIcon);
  glDeleteTextures(1, &uiTextures.zombiePortrait);
  glDeleteVertexArrays(1, &rangeCircleVAO);
  glDeleteBuffers(1, &rangeCircleVBO);
  deleteGpuMesh(grassMesh);
  deleteGpuMesh(curveMesh);
  deleteGpuMesh(skyMesh);
  deleteAllShaders(pipe);

  return 0;
}

}  // namespace

int main() {
  if (!glfwInit()) {
    return 1;
  }
  std::srand(static_cast<unsigned int>(std::time(NULL)));
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Tela cheia "borderless": janela SEM borda, do tamanho exato do monitor e
  // posicionada na origem dele. O shell do Windows trata isso como fullscreen e
  // mantém a barra de tarefas embaixo — ela não aparece mais por cima do jogo.
  // (Não usamos monitor != nullptr de propósito: fullscreen exclusivo minimiza
  //  e pisca no Alt+Tab; borderless troca de janela instantaneamente.)
  GLFWmonitor *monitor       = glfwGetPrimaryMonitor();
  const GLFWvidmode *mode    = glfwGetVideoMode(monitor);
  int monX = 0, monY = 0;
  glfwGetMonitorPos(monitor, &monX, &monY);
  glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

  GLFWwindow *window = glfwCreateWindow(
      mode->width, mode->height, render_constants::kWindowTitle, nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return 1;
  }
  glfwSetWindowPos(window, monX, monY);

  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }
  audio::init();
  const int exitCode = run(window);
  audio::shutdown();
  glfwDestroyWindow(window);
  glfwTerminate();
  return exitCode;
}
