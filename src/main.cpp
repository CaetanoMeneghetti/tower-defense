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
#include <ctime>
#include <iostream>
#include <memory>
#include <vector>

// ---- engine ----
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
#include "game/troop_placement.h"
#include "game/troop_selection.h"
#include "game/upgrade_system.h"

// ---- input ----
#include "input/camera_controller.h"
#include "input/input_handler.h"

// ---- render ----
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

  unsigned int castleColor;
  unsigned int castleNormal;

  unsigned int treeLog;
  unsigned int treeLeaves;
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
  t.archerNormal       = loadTexture("data/textures/archernormal.png");
  t.bowColor           = loadTexture("data/textures/bow.jpg");

  t.arquebusColor      = loadTexture("data/textures/arquebus.png");
  t.arquebusNormal     = loadTexture("data/textures/arquebusnormal.png");
  t.arquebusWeapon     = loadTexture("data/textures/arquebusweapon.png");

  t.castleColor        = loadTexture("data/textures/castle.png");
  t.castleNormal       = loadTexture("data/textures/castlenormal.png");

  t.treeLog            = loadTexture("data/textures/log.jpeg");
  t.treeLeaves         = loadTexture("data/textures/leaves.png", 4);

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
  };
  for (unsigned int tex : all) {
    if (tex != 0) glDeleteTextures(1, &tex);
  }
}

// =============================================================================
// PONTOS DE CONTROLE DA CURVA (path do nível)
// =============================================================================
// Simula f(x) = x * cos(x), x ∈ [-2, 2] -> [-10, 10] em 20 pontos.
std::vector<Point> defaultControlPoints() {
  return {
      {-10.0000f, -4.1615f}, {-8.9474f, -1.9551f}, {-7.8947f, -0.0643f},
      {-6.8421f, 1.3658f},   {-5.7895f, 2.3234f},  {-4.7368f, 2.7654f},
      {-3.6842f, 2.7286f},   {-2.6316f, 2.2750f},  {-1.5789f, 1.5009f},
      {-0.5263f, 0.5234f},   {0.5263f, -0.5234f},  {1.5789f, -1.5009f},
      {2.6316f, -2.2750f},   {3.6842f, -2.7286f},  {4.7368f, -2.7654f},
      {5.7895f, -2.3234f},   {6.8421f, -1.3658f},  {7.8947f, 0.0643f},
      {8.9474f, 1.9551f},    {10.0000f, 4.1615f},
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
  AnimatedModel enemyBase("data/models/zombie/zombieT.glb");
  enemyBase.loadAnimation("run", "data/models/zombie/zombieRun.glb");

  // Arcabuz — 5 tiers (sistema de upgrade)
  AnimatedModel arquebusBase("data/models/arquebus/arquebusT.glb");
  arquebusBase.loadAnimation("idle1", "data/models/arquebus/arquebusIdle.glb");
  AnimatedModel arquebusBaseLvl2("data/models/arquebus/arquebusT2.glb");
  arquebusBaseLvl2.loadAnimation("idle1", "data/models/arquebus/arquebusIdle.glb");
  AnimatedModel arquebusBaseLvl3("data/models/arquebus/arquebusT3.glb");
  arquebusBaseLvl3.loadAnimation("idle1", "data/models/arquebus/arquebusIdle.glb");
  AnimatedModel arquebusBaseLvl4("data/models/arquebus/arquebusT4.glb");
  arquebusBaseLvl4.loadAnimation("idle1", "data/models/arquebus/arquebusIdle.glb");
  AnimatedModel arquebusBaseLvl5("data/models/arquebus/arquebusT5.glb");
  arquebusBaseLvl5.loadAnimation("idle1", "data/models/arquebus/arquebusIdle.glb");

  // Arqueiro — 5 tiers (sistema de upgrade)
  AnimatedModel archerBase("data/models/Archer/ArcherT.glb");
  archerBase.loadAnimation("idle1", "data/models/Archer/Idle1.glb");
  archerBase.loadAnimation("idle2", "data/models/Archer/Idle2.glb");
  archerBase.loadAnimation("idle3", "data/models/Archer/Idle3.glb");
  archerBase.loadAnimation("idle4", "data/models/Archer/Idle4.glb");
  archerBase.loadAnimation("aim", "data/models/Archer/AimDraw.glb");
  AnimatedModel archerBaseLvl2("data/models/Archer/archerT2.glb");
  archerBaseLvl2.loadAnimation("idle1", "data/models/Archer/Idle1.glb");
  archerBaseLvl2.loadAnimation("idle2", "data/models/Archer/Idle2.glb");
  archerBaseLvl2.loadAnimation("aim", "data/models/Archer/AimDraw.glb");
  AnimatedModel archerBaseLvl3("data/models/Archer/archerT3.glb");
  archerBaseLvl3.loadAnimation("idle1", "data/models/Archer/Idle1.glb");
  archerBaseLvl3.loadAnimation("aim", "data/models/Archer/AimDraw.glb");
  AnimatedModel archerBaseLvl4("data/models/Archer/archerT4.glb");
  archerBaseLvl4.loadAnimation("idle1", "data/models/Archer/Idle1.glb");
  archerBaseLvl4.loadAnimation("aim", "data/models/Archer/AimDraw.glb");
  AnimatedModel archerBaseLvl5("data/models/Archer/archerT5.glb");
  archerBaseLvl5.loadAnimation("idle1", "data/models/Archer/Idle1.glb");
  archerBaseLvl5.loadAnimation("aim", "data/models/Archer/AimDraw.glb");

  std::vector<Vertex> bowVertices;
  if (!loadObj("data/models/Archer/bow.obj", bowVertices)) {
    std::cout << "ERRO: Nao encontrou data/models/Archer/bow.obj" << std::endl;
  }
  Mesh bowMesh(bowVertices);

  std::vector<Vertex> arquebusVertices;
  if (!loadObj("data/models/arquebus/arquebusweapon.obj", arquebusVertices)) {
    std::cout << "ERRO: Nao encontrou arquebusweapon.obj" << std::endl;
  }
  Mesh arquebusMesh(arquebusVertices);

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

  // ---------------------------------------------------------------------------
  // Estado de combate + iluminação
  // ---------------------------------------------------------------------------
  GameObject enemyRunner(&enemyClass, Vector<3>{0.0f, 0.0f, 0.0f});
  enemyRunner.setIdleAnimations({"run"});

  EnemyInstance enemy = makeEnemy(kZombieStats);
  std::vector<GameObject> defenders;
  std::vector<DefenderShoot> defenderShoots;

  // Pré-spawn de um arqueiro inicial (gameplay: o jogador começa com 1).
  defenders.push_back(GameObject(&archerClass, Vector<3>{0.0f, 0.1f, 0.0f}));
  defenders.back().setIdleAnimations({"idle1"});

  int selectedTroopIndex = -1;
  DirectionalLight moonLight = makeMoonLight();

  Camera cam;
  Vector<3> cameraPosition{0.0f, 2.0f, 5.0f};
  Matrix<4, 4> identity = Matrix<4, 4>::identity();

  // ---------------------------------------------------------------------------
  // LOOP PRINCIPAL
  // ---------------------------------------------------------------------------
  using namespace game_constants;
  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    double currentTime = glfwGetTime();
    if (currentTime - lastTime < kFrameDelay) {
      continue;
    }
    const float deltaTime = static_cast<float>(kFrameDelay);
    lastTime += kFrameDelay;

    // ------- UPDATE: inimigo + defensores -------
    EnemyTickResult enemyTick = updateEnemy(
        enemy, enemyRunner, curvePoints, curveCache, state, deltaTime);
    updateDefenders(defenders, defenderShoots, enemy, enemyTick.position, deltaTime);

    // ------- INPUT por frame -------
    processInput(window, cameraPosition, deltaTime);

    // ------- Clear + camera setup -------
    glClearColor(kFogColor.r, kFogColor.g, kFogColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float aspect = static_cast<float>(state.fbWidth) / static_cast<float>(state.fbHeight);
    cam.setPerspective(kFovDegrees * math_constants::kDegToRad, aspect, kNearPlane, kFarPlane);

    if (state.useFreeCamera) {
      cam.setFPS(cameraPosition, state.yaw, state.pitch);
    } else {
      updateOrbitalCameraPosition(state, cameraPosition);
      Vector<3> lookTarget{0.0f, 0.0f, 0.0f};
      cam.setLookAt(cameraPosition, lookTarget);
    }

    auto glView = toOpenGLMatrix(cam.getViewMatrix());
    auto glProj = toOpenGLMatrix(cam.getProjectionMatrix());
    const glm::vec3 glmViewPos(cameraPosition[0], cameraPosition[1], cameraPosition[2]);

    // ------- RENDER: entidades animadas (inimigo + defensores) -------
    uploadCommonUniforms(pipe.animShader, moonLight, lanternLights, glmViewPos, glView, glProj);
    glUniform1f(glGetUniformLocation(pipe.animShader, "hitFlash"), 0.0f);
    renderEnemy(pipe.animShader, enemy, enemyRunner, enemyTick.position, enemyTick.angle,
                tex.enemyColor, tex.defaultNormal);
    renderDefenders(pipe.animShader, defenders, tex.archerColor, tex.archerNormal);

    // ------- RENDER: armas dos defensores + castelo -------
    uploadCommonUniforms(pipe.objShader, moonLight, lanternLights, glmViewPos, glView, glProj);
    renderDefenderWeapons(pipe.objShader, pipe.objU, defenders, bowMesh, tex.bowColor);
    renderCastle(pipe.objShader, pipe.objU, castleMesh, tex.castleColor, tex.castleNormal,
                 curvePoints);

    // ------- RENDER: chão de grama -------
    uploadCommonUniforms(pipe.groundShader, moonLight, lanternLights, glmViewPos, glView, glProj);
    renderGround(grassMesh, tex.grass, tex.noise);

    // ------- RENDER: árvores -------
    uploadCommonUniforms(pipe.objShader, moonLight, lanternLights, glmViewPos, glView, glProj);
    renderTrees(pipe.objShader, pipe.objU, trees,
                treeLogMesh.get(), tex.treeLog,
                treeLeavesMesh.get(), tex.treeLeaves);

    // ------- RENDER: lanternas -------
    if (lanternMesh) {
      uploadCommonUniforms(pipe.lanternShader, moonLight, lanternLights, glmViewPos, glView, glProj);
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
    if (state.isPlacingTroop) {
      TroopPlacementContext ctx{
          window, cam, cameraPosition, curvePoints, pipe.previewShader,
          archerClass, arquebusClass, defenders, defenderShoots,
          glView.data(), glProj.data(),
      };
      handleTroopPlacement(ctx, state, deltaTime);
    }

    // ------- SELEÇÃO DE TROPA (clique no mundo) -------
    selectedTroopIndex = troop_selection::update(
        window, cam, cameraPosition, defenders, state, selectedTroopIndex);

    // ------- HUD + janela de upgrade (ImGui frame manual aqui pra empilhar) -------
    const float currentFps = 1.0f / deltaTime;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    gameHud.render(state, currentFps);

    if (selectedTroopIndex >= 0 && selectedTroopIndex < static_cast<int>(defenders.size())) {
      GameObject &troop = defenders[selectedTroopIndex];
      if (drawTroopDetailsHud(troop, state, uiTextures.archerIcon)) {
        troop.upgrade();
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

  GLFWwindow *window = glfwCreateWindow(
      render_constants::kWindowWidth, render_constants::kWindowHeight,
      render_constants::kWindowTitle, nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  const int exitCode = run(window);

  glfwDestroyWindow(window);
  glfwTerminate();
  return exitCode;
}
