#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <vector>
#include <string>
#include "engine/animatedmodel.h"
#include "math/matrix.h"
#include "math/vector.h"

class GameObject {
public:
    Vector<3> position;
    float rotationY;
    float scale;
    float animationTime;
    bool isActive;

    AnimatedModel* model;
    std::string currentAnimation;
    std::vector<std::string> idleAnimations;
    float stateTimer;
    float timeToNextIdle;

    GameObject(AnimatedModel* sharedModel, Vector<3> startPos);
    void Update(float deltaTime);
    void Draw(unsigned int shaderID);
    void SetAnimation(const std::string& animName);
    void SetIdleAnimations(const std::vector<std::string>& idles);
    glm::mat4 GetBoneWorldTransform(const std::string& boneName);
    
private:
    void PickRandomIdle();
};

#endif