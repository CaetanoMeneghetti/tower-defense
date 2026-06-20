#pragma once

struct ImFont;  // forward declaration (imgui.h not needed in header)

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "engine/animated_model.h"
#include "engine/camera.h"
#include "engine/game_object.h"
#include "engine/lighting.h"
#include "engine/mesh.h"
#include "engine/primitive_meshes.h"
#include "game/arrow_system.h"
#include "game/scene.h"
#include "math/vector.h"
#include "render/particle_system.h"
#include "render/scene_renderer.h"
#include "render/shader_uniforms.h"

// =============================================================================
// MAIN MENU
// =============================================================================

class MainMenu {
 public:
  bool run(GLFWwindow *window);

 private:
  // ---- ground (grama estática) -----------------------------------------------
  GLuint         groundShader_ = 0;
  GroundUniforms groundU_{};
  GpuMesh        groundMesh_;
  GrassTextures  grassTex_{};

  // ---- path de terra --------------------------------------------------------
  GLuint                pathShader_ = 0;
  PathUniforms          pathU_{};
  DirtTextures          dirtTex_{};
  GLuint                noiseTex_  = 0;
  std::unique_ptr<Mesh> pathMesh_;

  GLuint animShader_ = 0;

  // ---- trees ----------------------------------------------------------------
  GLuint                   treeShader_     = 0;
  GLuint                   treeLogTex_     = 0;
  GLuint                   treeLeavesTex_  = 0;
  std::unique_ptr<Mesh>    treeLogMesh_;
  std::unique_ptr<Mesh>    treeLeavesMesh_;
  std::vector<TreeInstance> trees_;
  GLuint                   treeInstanceVBO_ = 0;

  // ---- obj shader (weapons + ground static) ---------------------------------
  GLuint      objShader_ = 0;
  ObjUniforms objU_{};

  // ---- archer ---------------------------------------------------------------
  std::unique_ptr<AnimatedModel> archerModel_;
  GLuint archerTex_    = 0;
  GLuint archerNormal_ = 0;
  GLuint bowTex_       = 0;
  std::unique_ptr<Mesh> bowMesh_;
  std::unique_ptr<GameObject> archerProxy_;

  // ---- arquebus -------------------------------------------------------------
  std::unique_ptr<AnimatedModel> arquebusModel_;
  GLuint arquebusTex_       = 0;
  GLuint arquebusNormal_    = 0;
  GLuint arquebusWeaponTex_ = 0;
  std::unique_ptr<Mesh> arquebusMesh_;
  std::unique_ptr<GameObject> arquebusProxy_;

  // ---- knight ---------------------------------------------------------------
  std::unique_ptr<AnimatedModel> knightModel_;
  GLuint knightTex_      = 0;
  GLuint knightNormal_   = 0;
  GLuint attachmentTex_  = 0;  // sword + shield share this texture
  std::unique_ptr<Mesh> swordMesh_;
  std::unique_ptr<Mesh> shieldMesh_;
  std::unique_ptr<GameObject> knightProxy_;

  // ---- byzantine general ----------------------------------------------------
  std::unique_ptr<AnimatedModel> byzantineModel_;
  GLuint byzantineTex_    = 0;
  GLuint byzantineNormal_ = 0;
  std::unique_ptr<GameObject> byzantineProxy_;

  // ---- zombie ---------------------------------------------------------------
  std::unique_ptr<AnimatedModel> zombieModel_;
  GLuint zombieTex_    = 0;
  GLuint zombieNormal_ = 0;
  std::unique_ptr<GameObject> zombieProxy_;

  // ---- castle ---------------------------------------------------------------
  std::vector<CastleGroup> castleGroups_;
  GLuint                   castleDefaultNormal_ = 0;

  // ---- fontes ImGui ---------------------------------------------------------
  ImFont *titleFont_ = nullptr;  // Cinzel 48pt — usado só no título

  // ---- efeitos visuais (fumaça + flechas) -----------------------------------
  std::unique_ptr<CannonSmoke>    smoke_;
  GLuint                          particleShader_ = 0;
  GLuint                          smokeTex_       = 0;
  std::unique_ptr<Mesh>           arrowMesh_;
  GLuint                          arrowTex_       = 0;
  std::vector<ArrowProjectile>    menuArrows_;

  // ---- actors ---------------------------------------------------------------
  struct ArcherActor {
    float x, z;
    float animTime  = 0.0f;
    bool  aiming    = false;
    float aimTimer  = 0.0f;
    float idleCycle = 0.0f;
  };
  struct AquebusActor {
    float x, z;
    float animTime  = 0.0f;
    bool  firing    = false;
    float fireTimer = 0.0f;
    float cooldown  = 4.5f;
  };
  struct KnightActor {
    float x, z;
    float animTime    = 0.0f;
    bool  attacking   = false;
    float attackTimer = 0.0f;
  };
  struct GeneralActor {
    float animTime        = 0.0f;
    float commandTimer    = 4.0f;
    bool  commanding      = false;
    float commandDuration = 0.0f;
  };
  struct ZombieActor {
    float x, z;
    float startX, startZ;
    float animTime   = 0.0f;
    bool  dying      = false;
    float deathTimer = 0.0f;
  };

  std::vector<ArcherActor>  archers_;
  std::vector<AquebusActor> arquebus_;
  std::vector<KnightActor>  knights_;
  std::vector<ZombieActor>  zombies_;
  GeneralActor              general_;

  float killTimer_ = 3.0f;

  // ---- point lights (torches) -----------------------------------------------
  std::vector<PointLight> torchLights_;

  // ---- cinematic shots -------------------------------------------------------
  int   shot_      = 0;
  float shotTimer_ = 0.0f;
  float fadeAlpha_ = 1.0f;

  // ---- post-processing FBO --------------------------------------------------
  GLuint postShader_  = 0;
  GLuint fbo_         = 0;
  GLuint fboColorTex_ = 0;
  GLuint fboDepthRbo_ = 0;
  GLuint quadVAO_     = 0;
  GLuint quadVBO_     = 0;
  int    fboW_        = 0;
  int    fboH_        = 0;

  // ---- camera ---------------------------------------------------------------
  DirectionalLight moonLight_;
  Camera           cam_;
  float            sceneTime_ = 0.0f;

  void loadScene();
  void updateScene(float dt);
  void renderScene(int fbW, int fbH);
  void unloadScene();
  void ensureFbo(int w, int h);
  void renderUI(float W, float H, bool &outEnter, bool &outQuit);
};
