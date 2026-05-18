#include "engine/animated_model.h"

#include <glad/glad.h>

#include <iostream>

AnimatedModel::AnimatedModel(const std::string &path) {
  boneCounter_ = 0;
  loadModel(path);
}

AnimatedModel::~AnimatedModel() {
  glDeleteVertexArrays(1, &vao_);
  glDeleteBuffers(1, &vbo_);
  glDeleteBuffers(1, &ebo_);
}

void AnimatedModel::loadModel(const std::string &path) {
  scene_ = importer_.ReadFile(path,
                              aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                  aiProcess_JoinIdenticalVertices |
                                  aiProcess_LimitBoneWeights | aiProcess_FlipUVs |
                                  aiProcess_CalcTangentSpace);
  if (!scene_ || scene_->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene_->mRootNode) return;

  globalInverseTransform_ = glm::inverse(convertMatrixToGlmFormat(scene_->mRootNode->mTransformation));

  int vertexOffset = 0;
  for (unsigned int m = 0; m < scene_->mNumMeshes; m++) {
    aiMesh *mesh = scene_->mMeshes[m];
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
      VertexAnim vertex;
      setVertexBoneDataToDefault(vertex);
      vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
      vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
      if (mesh->HasTangentsAndBitangents()) {
        vertex.Tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
        vertex.Bitangent =
            glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
      } else {
        vertex.Tangent = glm::vec3(0.0f);
        vertex.Bitangent = glm::vec3(0.0f);
      }
      if (mesh->mTextureCoords[0]) {
        vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
      } else {
        vertex.TexCoords = glm::vec2(0.0f, 0.0f);
      }
      vertices_.push_back(vertex);
    }
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
      aiFace face = mesh->mFaces[i];
      for (unsigned int j = 0; j < face.mNumIndices; j++) {
        indices_.push_back(face.mIndices[j] + vertexOffset);
      }
    }
    extractBoneWeightForVertices(vertices_, mesh, scene_, vertexOffset);
    vertexOffset += mesh->mNumVertices;
  }
  setupMesh();
}

void AnimatedModel::loadAnimation(const std::string &name, const std::string &path) {
  auto importer = std::make_shared<Assimp::Importer>();
  const aiScene *scene = importer->ReadFile(path, aiProcess_LimitBoneWeights);
  if (scene && scene->HasAnimations()) {
    animImporters_[name] = importer;
    animations_[name] = scene;
  }
}

std::vector<glm::mat4> AnimatedModel::getTransformsAtTime(const std::string &animName,
                                                          float timeInSeconds) {
  const aiAnimation *animation = nullptr;
  if (animations_.count(animName)) {
    animation = animations_[animName]->mAnimations[0];
  } else if (scene_->HasAnimations()) {
    animation = scene_->mAnimations[0];
  }

  if (animation) {
    float tps = animation->mTicksPerSecond != 0 ? animation->mTicksPerSecond : 25.0f;
    float timeInTicks = fmod(timeInSeconds * tps, (float)animation->mDuration);
    calculateBoneTransform(scene_->mRootNode, glm::mat4(1.0f), animation, timeInTicks);
  }
  return finalBoneMatrices_;
}

void AnimatedModel::calculateBoneTransform(const aiNode *node,
                                           glm::mat4 parentTransform,
                                           const aiAnimation *animation,
                                           float time) {
  std::string nodeName = node->mName.data;
  glm::mat4 nodeTransform = convertMatrixToGlmFormat(node->mTransformation);
  const aiNodeAnim *nodeAnim = findNodeAnim(animation, nodeName);

  if (nodeAnim) {
    nodeTransform = interpolateTranslation(time, nodeAnim) *
                    interpolateRotation(time, nodeAnim) *
                    interpolateScaling(time, nodeAnim);
  }

  glm::mat4 globalTransformation = parentTransform * nodeTransform;
  globalNodeTransforms_[nodeName] = globalTransformation;

  if (boneInfoMap_.count(nodeName)) {
    int index = boneInfoMap_[nodeName].id;
    finalBoneMatrices_[index] =
        globalInverseTransform_ * globalTransformation * boneInfoMap_[nodeName].offset;
  }

  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    calculateBoneTransform(node->mChildren[i], globalTransformation, animation, time);
  }
}

glm::mat4 AnimatedModel::getNodeGlobalTransform(const std::string &nodeName) const {
  auto it = globalNodeTransforms_.find(nodeName);
  if (it != globalNodeTransforms_.end()) {
    return globalInverseTransform_ * it->second;
  }
  return glm::mat4(1.0f);
}

void AnimatedModel::extractBoneWeightForVertices(std::vector<VertexAnim> &vertices,
                                                 aiMesh *mesh,
                                                 const aiScene * /*scene*/,
                                                 int vertexOffset) {
  for (unsigned int i = 0; i < mesh->mNumBones; ++i) {
    int boneId = -1;
    std::string boneName = mesh->mBones[i]->mName.C_Str();
    if (boneInfoMap_.find(boneName) == boneInfoMap_.end()) {
      BoneInfo newBoneInfo;
      newBoneInfo.id = boneCounter_++;
      newBoneInfo.offset = convertMatrixToGlmFormat(mesh->mBones[i]->mOffsetMatrix);
      boneInfoMap_[boneName] = newBoneInfo;
      boneId = newBoneInfo.id;
    } else {
      boneId = boneInfoMap_[boneName].id;
    }

    auto weights = mesh->mBones[i]->mWeights;
    for (unsigned int j = 0; j < mesh->mBones[i]->mNumWeights; ++j) {
      int vertexId = weights[j].mVertexId + vertexOffset;
      setVertexBoneData(vertices[vertexId], boneId, weights[j].mWeight);
    }
  }
  if (finalBoneMatrices_.size() < 100) {
    finalBoneMatrices_.assign(100, glm::mat4(1.0f));
  }
}

void AnimatedModel::setVertexBoneDataToDefault(VertexAnim &vertex) {
  for (int i = 0; i < 4; i++) {
    vertex.m_BoneIDs[i] = 0;
    vertex.m_Weights[i] = 0.0f;
  }
}

void AnimatedModel::setVertexBoneData(VertexAnim &vertex, int boneId, float weight) {
  for (int i = 0; i < 4; ++i) {
    if (vertex.m_Weights[i] == 0.0f) {
      vertex.m_Weights[i] = weight;
      vertex.m_BoneIDs[i] = boneId;
      break;
    }
  }
}

void AnimatedModel::setupMesh() {
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glGenBuffers(1, &ebo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(VertexAnim), &vertices_[0],
               GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_.size() * sizeof(unsigned int), &indices_[0],
               GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAnim), (void *)offsetof(VertexAnim, Position));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(
      1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexAnim), (void *)offsetof(VertexAnim, TexCoords));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(
      2, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAnim), (void *)offsetof(VertexAnim, Normal));
  glEnableVertexAttribArray(3);
  glVertexAttribIPointer(
      3, 4, GL_INT, sizeof(VertexAnim), (void *)offsetof(VertexAnim, m_BoneIDs));
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(
      4, 4, GL_FLOAT, GL_FALSE, sizeof(VertexAnim), (void *)offsetof(VertexAnim, m_Weights));
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(
      5, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAnim), (void *)offsetof(VertexAnim, Tangent));
  glEnableVertexAttribArray(6);
  glVertexAttribPointer(
      6, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAnim), (void *)offsetof(VertexAnim, Bitangent));
  glBindVertexArray(0);
}

const aiNodeAnim *AnimatedModel::findNodeAnim(const aiAnimation *animation,
                                              const std::string &nodeName) {
  if (!animation) return nullptr;

  for (unsigned int i = 0; i < animation->mNumChannels; i++) {
    if (std::string(animation->mChannels[i]->mNodeName.data) == nodeName) {
      return animation->mChannels[i];
    }
  }

  // Fallback: tenta sem o prefixo "mixamorig:" caso o asset use namespace.
  auto cleanName = [](const std::string &name) {
    size_t pos = name.find(':');
    return (pos != std::string::npos) ? name.substr(pos + 1) : name;
  };

  std::string cleanTarget = cleanName(nodeName);
  for (unsigned int i = 0; i < animation->mNumChannels; i++) {
    std::string channelName = animation->mChannels[i]->mNodeName.data;
    if (cleanName(channelName) == cleanTarget) {
      return animation->mChannels[i];
    }
  }

  return nullptr;
}

glm::mat4 AnimatedModel::interpolateTranslation(float time, const aiNodeAnim *nodeAnim) {
  if (nodeAnim->mNumPositionKeys == 1) {
    return glm::translate(
        glm::mat4(1.0f),
        glm::vec3(nodeAnim->mPositionKeys[0].mValue.x, nodeAnim->mPositionKeys[0].mValue.y,
                  nodeAnim->mPositionKeys[0].mValue.z));
  }
  unsigned int i = 0;
  for (; i < nodeAnim->mNumPositionKeys - 1; i++) {
    if (time < (float)nodeAnim->mPositionKeys[i + 1].mTime) break;
  }
  float factor = (time - (float)nodeAnim->mPositionKeys[i].mTime) /
                 (float)(nodeAnim->mPositionKeys[i + 1].mTime - nodeAnim->mPositionKeys[i].mTime);
  aiVector3D v = nodeAnim->mPositionKeys[i].mValue +
                 factor * (nodeAnim->mPositionKeys[i + 1].mValue - nodeAnim->mPositionKeys[i].mValue);
  return glm::translate(glm::mat4(1.0f), glm::vec3(v.x, v.y, v.z));
}

glm::mat4 AnimatedModel::interpolateRotation(float time, const aiNodeAnim *nodeAnim) {
  if (nodeAnim->mNumRotationKeys == 1) {
    return glm::toMat4(glm::normalize(
        glm::quat(nodeAnim->mRotationKeys[0].mValue.w, nodeAnim->mRotationKeys[0].mValue.x,
                  nodeAnim->mRotationKeys[0].mValue.y, nodeAnim->mRotationKeys[0].mValue.z)));
  }
  unsigned int i = 0;
  for (; i < nodeAnim->mNumRotationKeys - 1; i++) {
    if (time < (float)nodeAnim->mRotationKeys[i + 1].mTime) break;
  }
  float factor = (time - (float)nodeAnim->mRotationKeys[i].mTime) /
                 (float)(nodeAnim->mRotationKeys[i + 1].mTime - nodeAnim->mRotationKeys[i].mTime);
  aiQuaternion q;
  aiQuaternion::Interpolate(
      q, nodeAnim->mRotationKeys[i].mValue, nodeAnim->mRotationKeys[i + 1].mValue, factor);
  return glm::toMat4(glm::normalize(glm::quat(q.w, q.x, q.y, q.z)));
}

glm::mat4 AnimatedModel::interpolateScaling(float time, const aiNodeAnim *nodeAnim) {
  if (nodeAnim->mNumScalingKeys == 1) {
    return glm::scale(
        glm::mat4(1.0f),
        glm::vec3(nodeAnim->mScalingKeys[0].mValue.x, nodeAnim->mScalingKeys[0].mValue.y,
                  nodeAnim->mScalingKeys[0].mValue.z));
  }
  unsigned int i = 0;
  for (; i < nodeAnim->mNumScalingKeys - 1; i++) {
    if (time < (float)nodeAnim->mScalingKeys[i + 1].mTime) break;
  }
  float factor = (time - (float)nodeAnim->mScalingKeys[i].mTime) /
                 (float)(nodeAnim->mScalingKeys[i + 1].mTime - nodeAnim->mScalingKeys[i].mTime);
  aiVector3D v = nodeAnim->mScalingKeys[i].mValue +
                 factor * (nodeAnim->mScalingKeys[i + 1].mValue - nodeAnim->mScalingKeys[i].mValue);
  return glm::scale(glm::mat4(1.0f), glm::vec3(v.x, v.y, v.z));
}

void AnimatedModel::draw(unsigned int /*shaderId*/) {
  glBindVertexArray(vao_);
  glDrawElements(GL_TRIANGLES, indices_.size(), GL_UNSIGNED_INT, 0);
}

glm::mat4 AnimatedModel::convertMatrixToGlmFormat(const aiMatrix4x4 &from) {
  glm::mat4 to;
  to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
  to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
  to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
  to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
  return to;
}
