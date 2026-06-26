#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoords;
layout(location = 2) in vec3 normal;
// locations 3-6: mat4 de instância; 7-9: mat3 normal pré-computada
layout(location = 3) in mat4 instanceModel;
layout(location = 7) in mat3 instanceNormal;

uniform mat4  view;
uniform mat4  projection;
uniform float time;

out vec2 vTexCoords;
out vec3 vFragPos;
out vec3 vNormal;

void main() {
    vec4 worldPos = instanceModel * vec4(position, 1.0);

    // Balanço procedural: vértices mais altos balançam mais (base ancorada em Y=0)
    float h     = max(position.y, 0.0);
    float swayX = sin(time * 2.2 + worldPos.x * 1.3 + worldPos.z * 0.9) * h * 0.12;
    float swayZ = sin(time * 1.8 + worldPos.x * 0.7 + worldPos.z * 1.4) * h * 0.06;
    worldPos.x += swayX;
    worldPos.z += swayZ;

    vFragPos   = worldPos.xyz;
    vNormal    = instanceNormal * normal;
    vTexCoords = texCoords;

    gl_Position = projection * view * worldPos;
}
