#pragma once

#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "engine/animated_model.h"
#include "engine/mesh.h"
#include "math/matrix.h"
#include "math/vector.h"

// =============================================================================
// GAME OBJECT — entidade animada com posição/rotação/escala + AnimatedModel
// =============================================================================
// Vários GameObjects podem compartilhar o mesmo AnimatedModel (apontam para
// o mesmo `model`). O `type` distingue famílias de defensores (1=arqueiro,
// 2=arcabuz, 3=inimigo). O sistema de tiers/upgrades vive em
// game/upgrade_system.h.

// Um tier específico de uma tropa (modelo + texturas + mesh de arma).
// Inicializadores garantem campos opcionais zero/identidade.
struct TroopTier {
  AnimatedModel *model = nullptr;
  unsigned int texture = 0;
  unsigned int normalMap = 0;
  Mesh *weaponMesh = nullptr;
  unsigned int weaponTexture = 0;
  glm::mat4 weaponOffset = glm::mat4(1.0f);
};

// Nível máximo de uma tropa (acima disso, sem upgrades disponíveis). O nível
// progride até aqui mesmo que a classe só tenha 1 tier de modelo (canhão,
// comandante) — a troca de mesh é condicional ao tier, o ++level não.
constexpr int kMaxTroopLevel = 5;

// Definição de classe (família) de tropa: type + lista de tiers (1..N).
struct TroopDef {
  int type = 0;  // 1 = Arqueiro, 2 = Arcabuz, 3 = Inimigo
  std::vector<TroopTier> tiers;
};

class GameObject {
 public:
  const TroopDef *baseDef = nullptr;

  Vector<3> position;
  float rotationY;
  float scale;
  float animationTime;
  bool isActive;
  int type = 1;

  // Estado de upgrade (modificado por game/upgrade_system.h).
  int level = 1;
  int damage = 10;
  float range = 15.0f;
  float fireRate = 2.0f;

  AnimatedModel *model;
  std::string currentAnimation;
  std::vector<std::string> idleAnimations;
  float stateTimer;
  float timeToNextIdle;
  bool  reverseAnim = false;  // se true, animationTime decrementa em update()
  bool  loopAnim    = true;   // false em clipes de ação (tiro/morte): tocam 1x e seguram

  GameObject(const TroopDef *def, Vector<3> startPos);

  void update(float deltaTime);
  void draw(unsigned int shaderId);
  // loop=true (padrão): idle/locomoção, repetem. loop=false: clipes de ação
  // (tiro, mira, morte, comando) tocam uma vez e seguram o último frame.
  void setAnimation(const std::string &animName, bool loop = true);
  // Inicia animação tocando de trás para frente (startTime = ponto de início,
  // tipicamente a duração da animação). Útil para efeitos de "desfazer".
  void setAnimationReverse(const std::string &animName, float startTime);
  void setIdleAnimations(const std::vector<std::string> &idles);
  glm::mat4 getBoneWorldTransform(const std::string &boneName);

  // Avança um nível: troca o `model` para `tiers[level-1].model` e reanima.
  void upgrade();
  // Retorna o tier ativo (faz clamp se level > tiers.size).
  const TroopTier &getCurrentTier() const;

 private:
  void pickRandomIdle();

  // --- Cache de pose por frame ---
  // Recalcular o esqueleto (AnimatedModel::getTransformsAtTime) é caro: faz uma
  // recursão pela árvore de nós com interpolação de quaternion. Antes isso rodava
  // 2-3x por frame para unidades com arma (uma no draw() e uma por mão no
  // getBoneWorldTransform()). A pose só depende de (model, animação, tempo, loop),
  // então memoizamos e só recomputamos quando uma dessas chaves muda — na prática
  // 1x/frame, logo após o update() avançar animationTime.
  //
  // poseNodes_ (snapshot do mapa de nós) só é preenchido para unidades que de fato
  // consultam bones de anexo (cacheNodes_); inimigos puros só usam poseBones_ e
  // não pagam a cópia do mapa.
  void ensurePose();

  std::vector<glm::mat4> poseBones_;
  std::map<std::string, glm::mat4> poseNodes_;
  glm::mat4 poseGlobalInverse_ = glm::mat4(1.0f);
  AnimatedModel *poseModel_ = nullptr;
  std::string poseAnim_;
  float poseTime_ = -1.0f;
  bool poseLoop_ = true;
  bool poseValid_ = false;
  bool cacheNodes_ = false;
};
