#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;
// locations 3-6 (tangent/bitangent/boneIDs/boneWeights) existem no VAO mas são ignoradas aqui

uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform float time;

out vec2 vTexCoords;
out vec3 vFragPos;
out vec3 vNormal;

void main() {
    vec4 worldPos = model * vec4(position, 1.0);

    // Balanço procedural: vértices mais altos balançam mais (base ancorada em Y=0)
    float h    = max(position.y, 0.0);
    float swayX = sin(time * 2.2 + worldPos.x * 1.3 + worldPos.z * 0.9) * h * 0.12;
    float swayZ = sin(time * 1.8 + worldPos.x * 0.7 + worldPos.z * 1.4) * h * 0.06;
    worldPos.x += swayX;
    worldPos.z += swayZ;

    vFragPos   = worldPos.xyz;
    vNormal    = mat3(transpose(inverse(model))) * normal;
    vTexCoords = texCoords;

    gl_Position = projection * view * worldPos;
}
