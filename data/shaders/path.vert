//executa 1 vez por vértice

#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTex;
layout (location = 2) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;
out vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
  gl_Position = projection * view * model * vec4(aPos, 1.0);
  FragPos  = vec3(model * vec4(aPos, 1.0));
  Normal   = normalize(mat3(transpose(inverse(model))) * aNormal);
  texCoord = aTex;
}
