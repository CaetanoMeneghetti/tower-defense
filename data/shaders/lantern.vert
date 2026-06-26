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
  vec4 worldPos = model * vec4(aPos, 1.0);
  gl_Position = projection * view * worldPos;
  FragPos  = vec3(worldPos);
  Normal   = normalize(mat3(transpose(inverse(model))) * aNormal);
  texCoord = aTex;
}
