#pragma once
#include <glm/glm.hpp>
#include <string>

#define MAX_BONE_INFLUENCE 4

struct VertexAnim {
    glm::vec3 Position;
    glm::vec2 TexCoords;
    glm::vec3 Normal;
    int m_BoneIDs[MAX_BONE_INFLUENCE];
    float m_Weights[MAX_BONE_INFLUENCE];
};

struct BoneInfo {
    int id;
    glm::mat4 offset;
};

struct AnimNode {
    glm::mat4 transformation;
    std::string name;
    int childrenCount;
    AnimNode** children;
};