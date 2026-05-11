#include "engine/gameobject.h"
#include "math/opengl_utils.h"
#include "math/constants.h"
#include "math/matrix_ops.h"
#include "math/transforms.h"
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>
#include <algorithm>
#include <cstdlib>

GameObject::GameObject(AnimatedModel* sharedModel, Vector<3> startPos) {
    model = sharedModel;
    position = startPos;
    rotationY = 0.0f;
    scale = 0.01f;
    animationTime = 0.0f;
    isActive = true;
    stateTimer = 0.0f;
    timeToNextIdle = 0.0f;
}

void GameObject::SetIdleAnimations(const std::vector<std::string>& idles) {
    idleAnimations = idles;
    if (!idleAnimations.empty()) {
        PickRandomIdle();
    }
}

void GameObject::SetAnimation(const std::string& animName) {
    if (currentAnimation != animName) {
        currentAnimation = animName;
        animationTime = 0.0f;
    }
    stateTimer = 0.0f;
}

void GameObject::PickRandomIdle() {
    if (idleAnimations.empty()) return;
    
    std::string nextAnim;
    
    do {
        int idx = rand() % idleAnimations.size();
        nextAnim = idleAnimations[idx];
    } while (idleAnimations.size() > 1 && nextAnim == currentAnimation);

    SetAnimation(nextAnim);
    timeToNextIdle = 3.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 4.0f));
}

void GameObject::Update(float deltaTime) {
    if (!isActive) return;
    animationTime += deltaTime;
    if (!idleAnimations.empty()) {
        stateTimer += deltaTime;
        if (stateTimer >= timeToNextIdle) {
            auto it = std::find(idleAnimations.begin(), idleAnimations.end(), currentAnimation);
            if (it != idleAnimations.end()) {
                PickRandomIdle();
            }
        }
    }
}

glm::mat4 GameObject::GetBoneWorldTransform(const std::string& boneName) {
    if (!model) return glm::mat4(1.0f);

    Matrix<4, 4> trans = translate<4, 4>(position[0], position[1], position[2]);
    Matrix<4, 4> rotY = rotateY<4, 4>(-rotationY);
    Matrix<4, 4> fixRotX = rotateX<4, 4>(math_constants::kHalfPi);
    Matrix<4, 4> scl = ::scale<4, 4>(scale, scale, scale);

    Matrix<4, 4> finalModel = trans * rotY * fixRotX * scl;
    
    auto glModelArray = toOpenGLMatrix(finalModel);
    glm::mat4 glmModel = glm::make_mat4(glModelArray.data());

    glm::mat4 boneTransform = model->GetNodeGlobalTransform(boneName);

    return glmModel * boneTransform;
}

void GameObject::Draw(unsigned int shaderID) {
    if (!isActive || !model) return;

    Matrix<4, 4> trans = translate<4, 4>(position[0], position[1], position[2]);
    Matrix<4, 4> rotY = rotateY<4, 4>(-rotationY);
    Matrix<4, 4> fixRotX = rotateX<4, 4>(math_constants::kHalfPi);
    Matrix<4, 4> scl = ::scale<4, 4>(scale, scale, scale);

    Matrix<4, 4> finalModel = trans * rotY * fixRotX * scl;
    
    auto glModel = toOpenGLMatrix(finalModel);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glModel.data());

    auto transforms = model->GetTransformsAtTime(currentAnimation, animationTime);

    if (!transforms.empty()) {
        for (int i = 0; i < (int)transforms.size(); ++i) {
            std::string n = "finalBonesMatrices[" + std::to_string(i) + "]";
            glUniformMatrix4fv(glGetUniformLocation(shaderID, n.c_str()), 1, GL_FALSE, glm::value_ptr(transforms[i]));
        }
    }

    model->Draw(shaderID);
}