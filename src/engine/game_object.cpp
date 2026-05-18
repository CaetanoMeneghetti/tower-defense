#include "engine/game_object.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdlib>

#include "math/constants.h"
#include "math/matrix_ops.h"
#include "math/opengl_utils.h"
#include "math/transforms.h"

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

  if (level < static_cast<int>(baseDef->tiers.size())) {
    ++level;
    model = baseDef->tiers[level - 1].model;

    // Reanima do início para reaplicar a transformação de bone sob o novo mesh.
    if (model) {
      std::string tempAnim = currentAnimation;
      currentAnimation = "";
      setAnimation(tempAnim);
      model->getTransformsAtTime(currentAnimation, 0.0f);
    }
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

void GameObject::setAnimation(const std::string &animName) {
  if (currentAnimation != animName) {
    currentAnimation = animName;
    animationTime = 0.0f;
  }
  stateTimer = 0.0f;
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

glm::mat4 GameObject::getBoneWorldTransform(const std::string &boneName) {
  if (!model) return glm::mat4(1.0f);

  Matrix<4, 4> trans = translate<4, 4>(position[0], position[1], position[2]);
  Matrix<4, 4> rotY = rotateY<4, 4>(-rotationY);
  Matrix<4, 4> fixRotX = rotateX<4, 4>(math_constants::kHalfPi);
  Matrix<4, 4> scl = ::scale<4, 4>(scale, scale, scale);
  Matrix<4, 4> finalModel = trans * rotY * fixRotX * scl;

  auto glModelArray = toOpenGLMatrix(finalModel);
  glm::mat4 glmModel = glm::make_mat4(glModelArray.data());

  glm::mat4 boneTransform = model->getNodeGlobalTransform(boneName);

  return glmModel * boneTransform;
}

void GameObject::draw(unsigned int shaderId) {
  if (!isActive || !model) return;

  Matrix<4, 4> trans = translate<4, 4>(position[0], position[1], position[2]);
  Matrix<4, 4> rotY = rotateY<4, 4>(-rotationY);
  Matrix<4, 4> fixRotX = rotateX<4, 4>(math_constants::kHalfPi);
  Matrix<4, 4> scl = ::scale<4, 4>(scale, scale, scale);
  Matrix<4, 4> finalModel = trans * rotY * fixRotX * scl;

  auto glModel = toOpenGLMatrix(finalModel);
  glUniformMatrix4fv(glGetUniformLocation(shaderId, "model"), 1, GL_FALSE, glModel.data());

  auto transforms = model->getTransformsAtTime(currentAnimation, animationTime);

  if (!transforms.empty()) {
    for (int i = 0; i < (int)transforms.size(); ++i) {
      std::string n = "finalBonesMatrices[" + std::to_string(i) + "]";
      glUniformMatrix4fv(
          glGetUniformLocation(shaderId, n.c_str()), 1, GL_FALSE, glm::value_ptr(transforms[i]));
    }
  }

  model->draw(shaderId);
}
