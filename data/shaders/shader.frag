#version 330 core

out vec4 fragColor;

in vec3 objNormal;

void main() {
  vec3 color = objNormal * 0.5 + 0.5;
  fragColor = vec4(color, 1.0);
}
