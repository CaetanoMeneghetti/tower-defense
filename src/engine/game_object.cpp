#include "engine/game_object.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdlib>

#include "math/constants.h"
#include "math/matrix_ops.h"
#include "math/opengl_utils.h"
#include "math/transforms.h"
#include "render/shader_uniforms.h"

GameObject::GameObject(const TroopDef *def, Vector<3> startPos) {
  baseDef = def;
  level = 1;
  model = (def && !def->tiers.empty()) ? def->tiers[0].model : nullptr;
  type = def ? def->type : 1;
  position = startPos;
  rotationY = 0.0f;
  scale = 0.01f;
  animationTime = 0.0f;
  isActive = true;
  stateTimer = 0.0f;
  timeToNextIdle = 0.0f;
}

void GameObject::upgrade() {
  if (!baseDef) return;

  // Nível sobe até kMaxTroopLevel mesmo sem tier novo: tropas com 1 só modelo
  // (canhão, comandante) ainda progridem em atributos. A troca de mesh é que é
  // condicional ao tier existir; o ++level não.
  if (level >= kMaxTroopLevel) return;
  ++level;

  const int tierCount = static_cast<int>(baseDef->tiers.size());
  AnimatedModel *nextModel = (level <= tierCount) ? baseDef->tiers[level - 1].model
                                                  : model;
  if (nextModel && nextModel != model) {
    model = nextModel;
    // Reanima do início para reaplicar a transformação de bone sob o novo mesh.
    std::string tempAnim = currentAnimation;
    currentAnimation = "";
    setAnimation(tempAnim);
    model->getTransformsAtTime(currentAnimation, 0.0f);
  }
}

const TroopTier &GameObject::getCurrentTier() const {
  int safeIndex = std::min(level - 1, static_cast<int>(baseDef->tiers.size()) - 1);
  return baseDef->tiers[safeIndex];
}

void GameObject::setIdleAnimations(const std::vector<std::string> &idles) {
  idleAnimations = idles;
  if (!idleAnimations.empty()) {
    pickRandomIdle();
  }
}

void GameObject::setAnimation(const std::string &animName, bool loop) {
  if (currentAnimation != animName) {
    currentAnimation = animName;
    animationTime    = 0.0f;
  }
  reverseAnim = false;
  loopAnim    = loop;
  stateTimer  = 0.0f;
}

void GameObject::setAnimationReverse(const std::string &animName, float startTime) {
  currentAnimation = animName;
  animationTime    = startTime;
  reverseAnim      = true;
  loopAnim         = false;  // clipe de ação tocado de trás pra frente, não repete
  stateTimer       = 0.0f;
}

void GameObject::pickRandomIdle() {
  if (idleAnimations.empty()) return;

  std::string nextAnim;
  do {
    int idx = rand() % idleAnimations.size();
    nextAnim = idleAnimations[idx];
  } while (idleAnimations.size() > 1 && nextAnim == currentAnimation);

  setAnimation(nextAnim);
  timeToNextIdle = 3.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 4.0f));
}

void GameObject::update(float deltaTime) {
  if (!isActive) return;
  if (reverseAnim)
    animationTime -= deltaTime;
  else
    animationTime += deltaTime;
  if (!idleAnimations.empty()) {
    stateTimer += deltaTime;
    if (stateTimer >= timeToNextIdle) {
      auto it = std::find(idleAnimations.begin(), idleAnimations.end(), currentAnimation);
      if (it != idleAnimations.end()) {
        pickRandomIdle();
      }
    }
  }
}

void GameObject::ensurePose() {
  if (!model) return;
  // Cache válido se nada que afeta o esqueleto mudou desde o último cálculo.
  if (poseValid_ && poseModel_ == model && poseLoop_ == loopAnim &&
      poseTime_ == animationTime && poseAnim_ == currentAnimation) {
    return;
  }
  // getTransformsAtTime preenche finalBoneMatrices_ (retornado) e, como efeito
  // colateral, globalNodeTransforms_ no modelo compartilhado. Tiramos o snapshot
  // imediatamente — antes que outro GO que use o mesmo modelo o sobrescreva.
  poseBones_ = model->getTransformsAtTime(currentAnimation, animationTime, loopAnim);
  if (cacheNodes_) {
    poseNodes_ = model->nodeTransforms();
    poseGlobalInverse_ = model->globalInverse();
  }
  poseModel_ = model;
  poseAnim_  = currentAnimation;
  poseTime_  = animationTime;
  poseLoop_  = loopAnim;
  poseValid_ = true;
}

glm::mat4 GameObject::getBoneWorldTransform(const std::string &boneName) {
  if (!model) return glm::mat4(1.0f);

  // Primeira consulta de bone neste GO: passa a snapshotar o mapa de nós e força
  // um recálculo para capturá-lo (inimigos puros nunca chegam aqui e não pagam
  // a cópia do mapa).
  if (!cacheNodes_) {
    cacheNodes_ = true;
    poseValid_ = false;
  }
  ensurePose();

  Matrix<4, 4> trans = translate<4, 4>(position[0], position[1], position[2]);
  Matrix<4, 4> rotY = rotateY<4, 4>(-rotationY);
  Matrix<4, 4> fixRotX = rotateX<4, 4>(math_constants::kHalfPi);
  Matrix<4, 4> scl = ::scale<4, 4>(scale, scale, scale);
  Matrix<4, 4> finalModel = trans * rotY * fixRotX * scl;

  auto glModelArray = toOpenGLMatrix(finalModel);
  glm::mat4 glmModel = glm::make_mat4(glModelArray.data());

  // Equivale ao antigo getNodeGlobalTransform: globalInverse * transform do nó,
  // ou identidade se o bone não existe — mas lido do snapshot, sem recursão.
  glm::mat4 boneTransform(1.0f);
  auto it = poseNodes_.find(boneName);
  if (it != poseNodes_.end()) boneTransform = poseGlobalInverse_ * it->second;

  return glmModel * boneTransform;
}

void GameObject::draw(unsigned int shaderId) {
  if (!isActive || !model) return;

  ensurePose();

  Matrix<4, 4> trans = translate<4, 4>(position[0], position[1], position[2]);
  Matrix<4, 4> rotY = rotateY<4, 4>(-rotationY);
  Matrix<4, 4> fixRotX = rotateX<4, 4>(math_constants::kHalfPi);
  Matrix<4, 4> scl = ::scale<4, 4>(scale, scale, scale);
  Matrix<4, 4> finalModel = trans * rotY * fixRotX * scl;

  auto glModel = toOpenGLMatrix(finalModel);
  glUniformMatrix4fv(cachedUniformLocation(shaderId, "model"), 1, GL_FALSE, glModel.data());

  if (!poseBones_.empty()) {
    // Sobe TODAS as matrizes de osso de uma vez: os elementos de um uniform array
    // têm locations sequenciais, então um único glUniformMatrix4fv com count=N
    // cobre o array inteiro. Antes isto era um loop de 100 ossos, cada iteração
    // com uma alocação de std::string + um glGetUniformLocation (lookup no driver)
    // + um upload — ~100x mais chamadas por personagem, por frame.
    glUniformMatrix4fv(cachedUniformLocation(shaderId, "finalBonesMatrices"),
                       static_cast<GLsizei>(poseBones_.size()), GL_FALSE,
                       glm::value_ptr(poseBones_[0]));
  }

  model->draw(shaderId);
}
