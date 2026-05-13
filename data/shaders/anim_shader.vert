#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 norm;
layout(location = 3) in ivec4 boneIds;
layout(location = 4) in vec4 weights;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

const int MAX_BONES          = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

out vec2 TexCoords;
out vec3 Normal;
out vec3 FragPos;

void main() {
    vec4 totalPosition = vec4(0.0);
    vec3 localNormal   = vec3(0.0);
    bool hasBones      = false;

    for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
        // Slot vazio tem peso 0
        if (weights[i] == 0.0) continue;
        if (boneIds[i] >= MAX_BONES) break;

        hasBones = true;

        vec4 localPos  = finalBonesMatrices[boneIds[i]] * vec4(pos, 1.0);
        totalPosition += localPos * weights[i];

        vec3 localNorm = mat3(finalBonesMatrices[boneIds[i]]) * norm;
        localNormal   += localNorm * weights[i];
    }

    if (!hasBones) {
        totalPosition = vec4(pos, 1.0);
        localNormal   = norm;
    }

    gl_Position = projection * view * model * totalPosition;
    TexCoords   = uv;
    // Normal matrix: corrige orientação das normais sob escala não-uniforme
    Normal  = normalize(mat3(transpose(inverse(model))) * localNormal);
    FragPos = vec3(model * totalPosition);
}
