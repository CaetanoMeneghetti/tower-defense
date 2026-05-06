#pragma once
#include "engine/anim_data.h"
#include <glad/glad.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <map>
#include <string>

class AnimatedModel {
public:
    AnimatedModel(const std::string& path);
    ~AnimatedModel();

    void Update(float deltaTime);
    void Draw(unsigned int shaderID);
    std::vector<glm::mat4> GetBoneTransforms();

private:
    unsigned int VAO, VBO, EBO;
    std::vector<VertexAnim> vertices;
    std::vector<unsigned int> indices;

    std::map<std::string, BoneInfo> m_BoneInfoMap;
    int m_BoneCounter = 0;
    
    const aiScene* m_Scene;
    Assimp::Importer m_Importer;
    
    float m_CurrentTime;
    float m_TicksPerSecond;
    float m_Duration;
    
    glm::mat4 m_GlobalInverseTransform;
    std::vector<glm::mat4> m_FinalBoneMatrices;

    void LoadModel(const std::string& path);
    void SetupMesh();
    void ExtractBoneWeightForVertices(std::vector<VertexAnim>& vertices, aiMesh* mesh, const aiScene* scene, int vertexOffset);;
    void SetVertexBoneData(VertexAnim& vertex, int boneID, float weight);
    void SetVertexBoneDataToDefault(VertexAnim& vertex);

    void CalculateBoneTransform(const aiNode* node, glm::mat4 parentTransform);
    const aiNodeAnim* FindNodeAnim(const aiAnimation* animation, const std::string& nodeName);
    
    glm::mat4 InterpolateTranslation(float animationTime, const aiNodeAnim* nodeAnim);
    glm::mat4 InterpolateRotation(float animationTime, const aiNodeAnim* nodeAnim);
    glm::mat4 InterpolateScaling(float animationTime, const aiNodeAnim* nodeAnim);

    glm::mat4 ConvertMatrixToGLMFormat(const aiMatrix4x4& from);
};