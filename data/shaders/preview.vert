#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 3) in ivec4 aBoneIds;
layout (location = 4) in vec4 aWeights;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const int MAX_BONES = 100;
uniform mat4 finalBonesMatrices[MAX_BONES];

void main() {
    vec4 totalPosition = vec4(0.0);
    for(int i = 0 ; i < 4 ; i++) {
        if(aBoneIds[i] == -1) continue;
        if(aBoneIds[i] >= MAX_BONES) break;
        
        vec4 localPosition = finalBonesMatrices[aBoneIds[i]] * vec4(aPos, 1.0);
        totalPosition += localPosition * aWeights[i];
    }
    
    
    if (totalPosition.w == 0.0) {
        totalPosition = vec4(aPos, 1.0);
    }

    gl_Position = projection * view * model * totalPosition;
}