#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 norm;
layout(location = 3) in ivec4 boneIds;
layout(location = 4) in vec4 weights;
layout(location = 5) in vec3 tangent;
layout(location = 6) in vec3 bitangent;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

const int MAX_BONES          = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

out vec2 TexCoords;
out vec3 Normal;
out vec3 FragPos;
out mat3 TBN;
out float vHasTangent;

void main() {
    vec4 totalPosition = vec4(0.0);
    vec3 localTangent  = vec3(0.0);
    vec3 localBitangent = vec3(0.0);
    vec3 localNormal   = vec3(0.0);
    bool hasBones      = false;

    for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
        // Slot vazio tem peso 0
        if (weights[i] == 0.0) continue;
        if (boneIds[i] >= MAX_BONES) break;

        hasBones = true;
        vec3 locBitan = mat3(finalBonesMatrices[boneIds[i]]) * bitangent;
        localBitangent += locBitan * weights[i];

        vec4 localPos  = finalBonesMatrices[boneIds[i]] * vec4(pos, 1.0);
        totalPosition += localPos * weights[i];

        vec3 localNorm = mat3(finalBonesMatrices[boneIds[i]]) * norm;
        localNormal   += localNorm * weights[i];

        vec3 locTan = mat3(finalBonesMatrices[boneIds[i]]) * tangent;
        localTangent += locTan * weights[i];
    }

    if (!hasBones) {
        totalPosition = vec4(pos, 1.0);
        localNormal   = norm;
        localTangent  = tangent;
        localBitangent = bitangent;
        
    }

    gl_Position = projection * view * model * totalPosition;
    TexCoords   = uv;
    // Normal matrix: corrige orientação das normais sob escala não-uniforme
    Normal  = normalize(mat3(transpose(inverse(model))) * localNormal);
    FragPos = vec3(model * totalPosition);
    //Nova parte para normal mapping
    
   if (length(localTangent) > 0.001 && length(localBitangent) > 0.001) {
        mat3 normalMatrix = mat3(transpose(inverse(model)));
        
        // Pega os vetores reais do C++ (que já vêm com o espelhamento e suavizados)
        vec3 T = normalize(normalMatrix * localTangent);
        vec3 B = normalize(normalMatrix * localBitangent);
        vec3 N = Normal;
        
        TBN = mat3(T, B, N);
        vHasTangent = 1.0; 
    } else {
        // Fallback seguro que não gera tela preta (NaN)
        TBN = mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), Normal);
        vHasTangent = 0.0; 
    }
    
}
