#pragma once

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

  GameObject(const TroopDef *def, Vector<3> startPos);

  void update(float deltaTime);
  void draw(unsigned int shaderId);
  void setAnimation(const std::string &animName);
  void setIdleAnimations(const std::vector<std::string> &idles);
  glm::mat4 getBoneWorldTransform(const std::string &boneName);

  // Avança um nível: troca o `model` para `tiers[level-1].model` e reanima.
  void upgrade();
  // Retorna o tier ativo (faz clamp se level > tiers.size).
  const TroopTier &getCurrentTier() const;

 private:
  void pickRandomIdle();
};
