#version 330 core

// Variante instanciada do shader OBJ padrão (shader.vert): a matriz de modelo
// vem de um atributo por-instância (locations 3..6, uma coluna cada) em vez do
// uniform "model". Usado para desenhar as ~1000 árvores em 1 draw call cada
// (tronco + folhagem) via glDrawArraysInstanced. Fragmento: reaproveita
// shader.frag (mesma iluminação/fog).

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTex;
layout (location = 2) in vec3 aNormal;
// Matriz de modelo por instância: 4 colunas de vec4 (mat4 column-major).
layout (location = 3) in vec4 aModelCol0;
layout (location = 4) in vec4 aModelCol1;
layout (location = 5) in vec4 aModelCol2;
layout (location = 6) in vec4 aModelCol3;

out vec3 FragPos;
out vec3 Normal;
out vec2 texCoord;

uniform mat4 view;
uniform mat4 projection;

void main() {
  mat4 model = mat4(aModelCol0, aModelCol1, aModelCol2, aModelCol3);
  gl_Position = projection * view * model * vec4(aPos, 1.0);
  FragPos  = vec3(model * vec4(aPos, 1.0));
  Normal   = normalize(mat3(transpose(inverse(model))) * aNormal);
  texCoord = aTex;
}
