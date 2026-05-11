#ifndef ANIMATEDMODEL_H
#define ANIMATEDMODEL_H

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct BoneInfo {
    int id;
    glm::mat4 offset;
};

struct VertexAnim {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    int m_BoneIDs[4];
    float m_Weights[4];
};

class AnimatedModel {
public:
    AnimatedModel(const std::string& path);
    ~AnimatedModel();

    glm::mat4 GetNodeGlobalTransform(const std::string& nodeName) const;
    void LoadAnimation(const std::string& name, const std::string& path);
    std::vector<glm::mat4> GetTransformsAtTime(const std::string& animName, float timeInSeconds);
    void Draw(unsigned int shaderID);

private:
    void LoadModel(const std::string& path);
    void SetupMesh();
    void ExtractBoneWeightForVertices(std::vector<VertexAnim>& vertices, aiMesh* mesh, const aiScene* scene, int vertexOffset);
    void SetVertexBoneDataToDefault(VertexAnim& vertex);
    void SetVertexBoneData(VertexAnim& vertex, int boneID, float weight);
    std::map<std::string, glm::mat4> m_GlobalNodeTransforms;
    
    // Agora recebe a animação e o tempo como parâmetros
    void CalculateBoneTransform(const aiNode* node, glm::mat4 parentTransform, const aiAnimation* animation, float time);
    const aiNodeAnim* FindNodeAnim(const aiAnimation* animation, const std::string& nodeName);
    
    glm::mat4 InterpolateTranslation(float time, const aiNodeAnim* nodeAnim);
    glm::mat4 InterpolateRotation(float time, const aiNodeAnim* nodeAnim);
    glm::mat4 InterpolateScaling(float time, const aiNodeAnim* nodeAnim);
    glm::mat4 ConvertMatrixToGLMFormat(const aiMatrix4x4& from);

    unsigned int VAO, VBO, EBO;
    std::vector<VertexAnim> vertices;
    std::vector<unsigned int> indices;
    
    Assimp::Importer m_Importer;
    const aiScene* m_Scene;
    glm::mat4 m_GlobalInverseTransform;
    
    std::map<std::string, BoneInfo> m_BoneInfoMap;
    int m_BoneCounter = 0;
    std::vector<glm::mat4> m_FinalBoneMatrices;

    // Dicionário para armazenar múltiplas animações
    std::map<std::string, std::shared_ptr<Assimp::Importer>> m_AnimImporters;
    std::map<std::string, const aiScene*> m_Animations;
};

#endif