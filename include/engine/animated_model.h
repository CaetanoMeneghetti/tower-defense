#pragma once

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

// =============================================================================
// ANIMATED MODEL — carregamento + skinning via Assimp
// =============================================================================
// Suporta múltiplas animações (LoadAnimation por nome) sobre um mesh único.
// A enumeração dos vértices/bones reflete o formato Assimp; nomes mantidos
// próximos ao original para evitar drift com material de referência.

struct BoneInfo {
  int id;
  glm::mat4 offset;
};

struct VertexAnim {
  glm::vec3 Position;
  glm::vec3 Normal;
  glm::vec2 TexCoords;
  glm::vec3 Tangent;
  glm::vec3 Bitangent;
  int m_BoneIDs[4];
  float m_Weights[4];
};

class AnimatedModel {
 public:
  AnimatedModel(const std::string &path);
  ~AnimatedModel();

  glm::mat4 getNodeGlobalTransform(const std::string &nodeName) const;
  void loadAnimation(const std::string &name, const std::string &path);
  std::vector<glm::mat4> getTransformsAtTime(const std::string &animName, float timeInSeconds);
  void draw(unsigned int shaderId);

 private:
  void loadModel(const std::string &path);
  void setupMesh();
  void extractBoneWeightForVertices(
      std::vector<VertexAnim> &vertices, aiMesh *mesh, const aiScene *scene, int vertexOffset);
  void setVertexBoneDataToDefault(VertexAnim &vertex);
  void setVertexBoneData(VertexAnim &vertex, int boneId, float weight);

  void calculateBoneTransform(
      const aiNode *node, glm::mat4 parentTransform, const aiAnimation *animation, float time);
  const aiNodeAnim *findNodeAnim(const aiAnimation *animation, const std::string &nodeName);

  glm::mat4 interpolateTranslation(float time, const aiNodeAnim *nodeAnim);
  glm::mat4 interpolateRotation(float time, const aiNodeAnim *nodeAnim);
  glm::mat4 interpolateScaling(float time, const aiNodeAnim *nodeAnim);
  glm::mat4 convertMatrixToGlmFormat(const aiMatrix4x4 &from);

  unsigned int vao_ = 0;
  unsigned int vbo_ = 0;
  unsigned int ebo_ = 0;
  std::vector<VertexAnim> vertices_;
  std::vector<unsigned int> indices_;

  Assimp::Importer importer_;
  const aiScene *scene_ = nullptr;
  glm::mat4 globalInverseTransform_;

  std::map<std::string, glm::mat4> globalNodeTransforms_;
  std::map<std::string, BoneInfo> boneInfoMap_;
  int boneCounter_ = 0;
  std::vector<glm::mat4> finalBoneMatrices_;

  // Dicionário para armazenar múltiplas animações
  std::map<std::string, std::shared_ptr<Assimp::Importer>> animImporters_;
  std::map<std::string, const aiScene *> animations_;
};
