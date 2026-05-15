#include "engine/animatedmodel.h"
#include <glad/glad.h>
#include <iostream>

AnimatedModel::AnimatedModel(const std::string& path) {
    m_BoneCounter = 0;
    LoadModel(path);
}

AnimatedModel::~AnimatedModel() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void AnimatedModel::LoadModel(const std::string& path) {
    m_Scene = m_Importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights | aiProcess_FlipUVs);
    if (!m_Scene || m_Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !m_Scene->mRootNode) return;
    
    m_GlobalInverseTransform = glm::inverse(ConvertMatrixToGLMFormat(m_Scene->mRootNode->mTransformation));
    
    int vertexOffset = 0;
    for (unsigned int m = 0; m < m_Scene->mNumMeshes; m++) {
        aiMesh* mesh = m_Scene->mMeshes[m];
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            VertexAnim vertex;
            SetVertexBoneDataToDefault(vertex);
            vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            if (mesh->mTextureCoords[0]) vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            else vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            vertices.push_back(vertex);
        }
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) indices.push_back(face.mIndices[j] + vertexOffset);
        }
        ExtractBoneWeightForVertices(vertices, mesh, m_Scene, vertexOffset);
        vertexOffset += mesh->mNumVertices;
    }
    SetupMesh();
}

void AnimatedModel::LoadAnimation(const std::string& name, const std::string& path) {
    auto importer = std::make_shared<Assimp::Importer>();
    const aiScene* scene = importer->ReadFile(path, aiProcess_LimitBoneWeights);
    if (scene && scene->HasAnimations()) {
        m_AnimImporters[name] = importer;
        m_Animations[name] = scene;
    }
}

std::vector<glm::mat4> AnimatedModel::GetTransformsAtTime(const std::string& animName, float timeInSeconds) {
    const aiAnimation* animation = nullptr;
    if (m_Animations.count(animName)) animation = m_Animations[animName]->mAnimations[0];
    else if (m_Scene->HasAnimations()) animation = m_Scene->mAnimations[0];
    
    if (animation) {
        float tps = animation->mTicksPerSecond != 0 ? animation->mTicksPerSecond : 25.0f;
        float timeInTicks = fmod(timeInSeconds * tps, (float)animation->mDuration);
        CalculateBoneTransform(m_Scene->mRootNode, glm::mat4(1.0f), animation, timeInTicks);
    }
    return m_FinalBoneMatrices;
}

void AnimatedModel::CalculateBoneTransform(const aiNode* node, glm::mat4 parentTransform, const aiAnimation* animation, float time) {
    std::string nodeName = node->mName.data;
    glm::mat4 nodeTransform = ConvertMatrixToGLMFormat(node->mTransformation);
    const aiNodeAnim* nodeAnim = FindNodeAnim(animation, nodeName);
    
    if (nodeAnim) {
        nodeTransform = InterpolateTranslation(time, nodeAnim) * InterpolateRotation(time, nodeAnim) * InterpolateScaling(time, nodeAnim);
    }
    
    glm::mat4 globalTransformation = parentTransform * nodeTransform;
    
    m_GlobalNodeTransforms[nodeName] = globalTransformation;
    
    if (m_BoneInfoMap.count(nodeName)) {
        int index = m_BoneInfoMap[nodeName].id;
        m_FinalBoneMatrices[index] = m_GlobalInverseTransform * globalTransformation * m_BoneInfoMap[nodeName].offset;
    }
    
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        CalculateBoneTransform(node->mChildren[i], globalTransformation, animation, time);
    }
}

glm::mat4 AnimatedModel::GetNodeGlobalTransform(const std::string& nodeName) const {
    auto it = m_GlobalNodeTransforms.find(nodeName);
    if (it != m_GlobalNodeTransforms.end()) {
        return m_GlobalInverseTransform * it->second;
    }
    return glm::mat4(1.0f);
}

void AnimatedModel::ExtractBoneWeightForVertices(std::vector<VertexAnim>& vertices, aiMesh* mesh, const aiScene* scene, int vertexOffset) {
    for (unsigned int i = 0; i < mesh->mNumBones; ++i) {
        int boneID = -1;
        std::string boneName = mesh->mBones[i]->mName.C_Str();
        if (m_BoneInfoMap.find(boneName) == m_BoneInfoMap.end()) {
            BoneInfo newBoneInfo;
            newBoneInfo.id = m_BoneCounter++;
            newBoneInfo.offset = ConvertMatrixToGLMFormat(mesh->mBones[i]->mOffsetMatrix);
            m_BoneInfoMap[boneName] = newBoneInfo;
            boneID = newBoneInfo.id;
        } else boneID = m_BoneInfoMap[boneName].id;
        
        auto weights = mesh->mBones[i]->mWeights;
        for (unsigned int j = 0; j < mesh->mBones[i]->mNumWeights; ++j) {
            int vertexId = weights[j].mVertexId + vertexOffset;
            SetVertexBoneData(vertices[vertexId], boneID, weights[j].mWeight);
        }
    }
    if (m_FinalBoneMatrices.size() < 100) m_FinalBoneMatrices.assign(100, glm::mat4(1.0f));
}

void AnimatedModel::SetVertexBoneDataToDefault(VertexAnim& vertex) {
    for (int i = 0; i < 4; i++) { vertex.m_BoneIDs[i] = 0; vertex.m_Weights[i] = 0.0f; }
}

void AnimatedModel::SetVertexBoneData(VertexAnim& vertex, int boneID, float weight) {
    for (int i = 0; i < 4; ++i) {
        if (vertex.m_Weights[i] == 0.0f) { vertex.m_Weights[i] = weight; vertex.m_BoneIDs[i] = boneID; break; }
    }
}

void AnimatedModel::SetupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VertexAnim), &vertices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAnim), (void*)offsetof(VertexAnim, Position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexAnim), (void*)offsetof(VertexAnim, TexCoords));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAnim), (void*)offsetof(VertexAnim, Normal));
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 4, GL_INT, sizeof(VertexAnim), (void*)offsetof(VertexAnim, m_BoneIDs));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(VertexAnim), (void*)offsetof(VertexAnim, m_Weights));
    glBindVertexArray(0);
}

const aiNodeAnim* AnimatedModel::FindNodeAnim(const aiAnimation* animation, const std::string& nodeName) {
    if (!animation) return nullptr;


    for (unsigned int i = 0; i < animation->mNumChannels; i++) {
        if (std::string(animation->mChannels[i]->mNodeName.data) == nodeName) {
            return animation->mChannels[i];
        }
    }

    
    auto cleanName = [](const std::string& name) {
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

glm::mat4 AnimatedModel::InterpolateTranslation(float time, const aiNodeAnim* nodeAnim) {
    if (nodeAnim->mNumPositionKeys == 1) return glm::translate(glm::mat4(1.0f), glm::vec3(nodeAnim->mPositionKeys[0].mValue.x, nodeAnim->mPositionKeys[0].mValue.y, nodeAnim->mPositionKeys[0].mValue.z));
    unsigned int i = 0;
    for (; i < nodeAnim->mNumPositionKeys - 1; i++) if (time < (float)nodeAnim->mPositionKeys[i + 1].mTime) break;
    float factor = (time - (float)nodeAnim->mPositionKeys[i].mTime) / (float)(nodeAnim->mPositionKeys[i+1].mTime - nodeAnim->mPositionKeys[i].mTime);
    aiVector3D v = nodeAnim->mPositionKeys[i].mValue + factor * (nodeAnim->mPositionKeys[i+1].mValue - nodeAnim->mPositionKeys[i].mValue);
    return glm::translate(glm::mat4(1.0f), glm::vec3(v.x, v.y, v.z));
}

glm::mat4 AnimatedModel::InterpolateRotation(float time, const aiNodeAnim* nodeAnim) {
    if (nodeAnim->mNumRotationKeys == 1) return glm::toMat4(glm::normalize(glm::quat(nodeAnim->mRotationKeys[0].mValue.w, nodeAnim->mRotationKeys[0].mValue.x, nodeAnim->mRotationKeys[0].mValue.y, nodeAnim->mRotationKeys[0].mValue.z)));
    unsigned int i = 0;
    for (; i < nodeAnim->mNumRotationKeys - 1; i++) if (time < (float)nodeAnim->mRotationKeys[i + 1].mTime) break;
    float factor = (time - (float)nodeAnim->mRotationKeys[i].mTime) / (float)(nodeAnim->mRotationKeys[i+1].mTime - nodeAnim->mRotationKeys[i].mTime);
    aiQuaternion q; aiQuaternion::Interpolate(q, nodeAnim->mRotationKeys[i].mValue, nodeAnim->mRotationKeys[i+1].mValue, factor);
    return glm::toMat4(glm::normalize(glm::quat(q.w, q.x, q.y, q.z)));
}

glm::mat4 AnimatedModel::InterpolateScaling(float time, const aiNodeAnim* nodeAnim) {
    if (nodeAnim->mNumScalingKeys == 1) return glm::scale(glm::mat4(1.0f), glm::vec3(nodeAnim->mScalingKeys[0].mValue.x, nodeAnim->mScalingKeys[0].mValue.y, nodeAnim->mScalingKeys[0].mValue.z));
    unsigned int i = 0;
    for (; i < nodeAnim->mNumScalingKeys - 1; i++) if (time < (float)nodeAnim->mScalingKeys[i + 1].mTime) break;
    float factor = (time - (float)nodeAnim->mScalingKeys[i].mTime) / (float)(nodeAnim->mScalingKeys[i+1].mTime - nodeAnim->mScalingKeys[i].mTime);
    aiVector3D v = nodeAnim->mScalingKeys[i].mValue + factor * (nodeAnim->mScalingKeys[i+1].mValue - nodeAnim->mScalingKeys[i].mValue);
    return glm::scale(glm::mat4(1.0f), glm::vec3(v.x, v.y, v.z));
}

void AnimatedModel::Draw(unsigned int shaderID) {
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
}

glm::mat4 AnimatedModel::ConvertMatrixToGLMFormat(const aiMatrix4x4& from) {
    glm::mat4 to;
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
}