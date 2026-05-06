#include "engine/animatedmodel.h"
#include <cstddef>
AnimatedModel::AnimatedModel(const std::string& path) {
    m_CurrentTime = 0.0f;
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

    if (m_Scene->HasAnimations()) {
        m_Duration = m_Scene->mAnimations[0]->mDuration;
        m_TicksPerSecond = m_Scene->mAnimations[0]->mTicksPerSecond != 0 ? m_Scene->mAnimations[0]->mTicksPerSecond : 25.0f;
    }

    int vertexOffset = 0; // Guarda a posição dos vértices das peças anteriores

    // Loop que varre todas as peças de roupa/corpo do personagem
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
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                // Soma o offset para não sobrescrever a peça anterior
                indices.push_back(face.mIndices[j] + vertexOffset); 
            }
        }

        ExtractBoneWeightForVertices(vertices, mesh, m_Scene, vertexOffset);
        vertexOffset += mesh->mNumVertices;
    }

    SetupMesh();
}

void AnimatedModel::ExtractBoneWeightForVertices(std::vector<VertexAnim>& vertices, aiMesh* mesh, const aiScene* scene, int vertexOffset) {
    for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
        int boneID = -1;
        std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();

        if (m_BoneInfoMap.find(boneName) == m_BoneInfoMap.end()) {
            BoneInfo newBoneInfo;
            newBoneInfo.id = m_BoneCounter;
            newBoneInfo.offset = ConvertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix);
            m_BoneInfoMap[boneName] = newBoneInfo;
            boneID = m_BoneCounter;
            m_BoneCounter++;
        } else {
            boneID = m_BoneInfoMap[boneName].id;
        }

        auto weights = mesh->mBones[boneIndex]->mWeights;
        int numWeights = mesh->mBones[boneIndex]->mNumWeights;

        for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex) {
            // O peso do osso precisa ser aplicado no vértice correto da malha atual!
            int vertexId = weights[weightIndex].mVertexId + vertexOffset; 
            float weight = weights[weightIndex].mWeight;
            SetVertexBoneData(vertices[vertexId], boneID, weight);
        }
    }
    m_FinalBoneMatrices.assign(100, glm::mat4(1.0f));
}

void AnimatedModel::SetVertexBoneDataToDefault(VertexAnim& vertex) {
    for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
        vertex.m_BoneIDs[i] = -1;
        vertex.m_Weights[i] = 0.0f;
    }
}

void AnimatedModel::SetVertexBoneData(VertexAnim& vertex, int boneID, float weight) {
    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
        if (vertex.m_BoneIDs[i] < 0) {
            vertex.m_Weights[i] = weight;
            vertex.m_BoneIDs[i] = boneID;
            break;
        }
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

void AnimatedModel::Update(float deltaTime) {
    if (!m_Scene || !m_Scene->HasAnimations()) return;

    m_CurrentTime += m_TicksPerSecond * deltaTime;
    m_CurrentTime = fmod(m_CurrentTime, m_Duration);

    CalculateBoneTransform(m_Scene->mRootNode, glm::mat4(1.0f));
}

void AnimatedModel::CalculateBoneTransform(const aiNode* node, glm::mat4 parentTransform) {
    std::string nodeName = node->mName.data;
    glm::mat4 nodeTransform = ConvertMatrixToGLMFormat(node->mTransformation);

    const aiAnimation* animation = m_Scene->mAnimations[0];
    const aiNodeAnim* nodeAnim = FindNodeAnim(animation, nodeName);

    if (nodeAnim) {
        glm::mat4 vecTrans = InterpolateTranslation(m_CurrentTime, nodeAnim);
        glm::mat4 rotTrans = InterpolateRotation(m_CurrentTime, nodeAnim);
        glm::mat4 scaleTrans = InterpolateScaling(m_CurrentTime, nodeAnim);
        nodeTransform = vecTrans * rotTrans * scaleTrans;
    }

    glm::mat4 globalTransformation = parentTransform * nodeTransform;

    if (m_BoneInfoMap.find(nodeName) != m_BoneInfoMap.end()) {
        int index = m_BoneInfoMap[nodeName].id;
        glm::mat4 offset = m_BoneInfoMap[nodeName].offset;
        m_FinalBoneMatrices[index] = m_GlobalInverseTransform * globalTransformation * offset;
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        CalculateBoneTransform(node->mChildren[i], globalTransformation);
    }
}

const aiNodeAnim* AnimatedModel::FindNodeAnim(const aiAnimation* animation, const std::string& nodeName) {
    for (unsigned int i = 0; i < animation->mNumChannels; i++) {
        const aiNodeAnim* nodeAnim = animation->mChannels[i];
        if (std::string(nodeAnim->mNodeName.data) == nodeName) return nodeAnim;
    }
    return nullptr;
}

glm::mat4 AnimatedModel::InterpolateTranslation(float animationTime, const aiNodeAnim* nodeAnim) {
    if (nodeAnim->mNumPositionKeys == 1) {
        auto pos = nodeAnim->mPositionKeys[0].mValue;
        return glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, pos.z));
    }
    
    unsigned int p0Index = 0;
    for (unsigned int i = 0; i < nodeAnim->mNumPositionKeys - 1; i++) {
        if (animationTime < (float)nodeAnim->mPositionKeys[i + 1].mTime) {
            p0Index = i;
            break;
        }
    }
    unsigned int p1Index = p0Index + 1;
    float deltaTime = (float)(nodeAnim->mPositionKeys[p1Index].mTime - nodeAnim->mPositionKeys[p0Index].mTime);
    float factor = (animationTime - (float)nodeAnim->mPositionKeys[p0Index].mTime) / deltaTime;
    
    aiVector3D start = nodeAnim->mPositionKeys[p0Index].mValue;
    aiVector3D end = nodeAnim->mPositionKeys[p1Index].mValue;
    aiVector3D delta = end - start;
    aiVector3D finalVec = start + factor * delta;
    
    return glm::translate(glm::mat4(1.0f), glm::vec3(finalVec.x, finalVec.y, finalVec.z));
}

glm::mat4 AnimatedModel::InterpolateRotation(float animationTime, const aiNodeAnim* nodeAnim) {
    if (nodeAnim->mNumRotationKeys == 1) {
        auto rot = nodeAnim->mRotationKeys[0].mValue;
        glm::quat q(rot.w, rot.x, rot.y, rot.z);
        return glm::toMat4(glm::normalize(q));
    }

    unsigned int p0Index = 0;
    for (unsigned int i = 0; i < nodeAnim->mNumRotationKeys - 1; i++) {
        if (animationTime < (float)nodeAnim->mRotationKeys[i + 1].mTime) {
            p0Index = i;
            break;
        }
    }
    unsigned int p1Index = p0Index + 1;
    float deltaTime = (float)(nodeAnim->mRotationKeys[p1Index].mTime - nodeAnim->mRotationKeys[p0Index].mTime);
    float factor = (animationTime - (float)nodeAnim->mRotationKeys[p0Index].mTime) / deltaTime;
    
    aiQuaternion start = nodeAnim->mRotationKeys[p0Index].mValue;
    aiQuaternion end = nodeAnim->mRotationKeys[p1Index].mValue;
    aiQuaternion finalQuat;
    aiQuaternion::Interpolate(finalQuat, start, end, factor);
    finalQuat.Normalize();
    
    glm::quat q(finalQuat.w, finalQuat.x, finalQuat.y, finalQuat.z);
    return glm::toMat4(q);
}

glm::mat4 AnimatedModel::InterpolateScaling(float animationTime, const aiNodeAnim* nodeAnim) {
    if (nodeAnim->mNumScalingKeys == 1) {
        auto scale = nodeAnim->mScalingKeys[0].mValue;
        return glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));
    }

    unsigned int p0Index = 0;
    for (unsigned int i = 0; i < nodeAnim->mNumScalingKeys - 1; i++) {
        if (animationTime < (float)nodeAnim->mScalingKeys[i + 1].mTime) {
            p0Index = i;
            break;
        }
    }
    unsigned int p1Index = p0Index + 1;
    float deltaTime = (float)(nodeAnim->mScalingKeys[p1Index].mTime - nodeAnim->mScalingKeys[p0Index].mTime);
    float factor = (animationTime - (float)nodeAnim->mScalingKeys[p0Index].mTime) / deltaTime;
    
    aiVector3D start = nodeAnim->mScalingKeys[p0Index].mValue;
    aiVector3D end = nodeAnim->mScalingKeys[p1Index].mValue;
    aiVector3D delta = end - start;
    aiVector3D finalVec = start + factor * delta;
    
    return glm::scale(glm::mat4(1.0f), glm::vec3(finalVec.x, finalVec.y, finalVec.z));
}

std::vector<glm::mat4> AnimatedModel::GetBoneTransforms() {
    return m_FinalBoneMatrices;
}

void AnimatedModel::Draw(unsigned int shaderID) {
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

glm::mat4 AnimatedModel::ConvertMatrixToGLMFormat(const aiMatrix4x4& from) {
    glm::mat4 to;
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
}