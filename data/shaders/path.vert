// Vertex shader do caminho de terra. Roda 1x por vértice.

#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTex;
// location 2 (normal do mesh) é ignorado de propósito: o caminho é sempre
// horizontal, então o fragment usa um TBN fixo + normal map para a iluminação.

out vec3 FragPos;
out vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
  vec4 worldPos = model * vec4(aPos, 1.0);
  FragPos     = worldPos.xyz;
  texCoord    = aTex;
  gl_Position = projection * view * worldPos;
}
