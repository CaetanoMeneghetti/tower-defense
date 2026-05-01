#version 330 core

// Entrada da posição do vértice (vem do VBO)
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 texCoord;

// Matrizes de transformação vindas da CPU
uniform mat4 view;
uniform mat4 projection;

void main() {
  // Aplica view (câmera) e projeção (tela)
  gl_Position = projection * view * vec4(aPos, 1.0);
  texCoord = aTexCoord;
}
