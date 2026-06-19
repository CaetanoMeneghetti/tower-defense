#version 330 core

layout(location = 0) in vec2 aPos;      // quad local (-0.5..0.5)
layout(location = 1) in vec2 aTexCoord; // UV do frame completo (0..1)

uniform mat4  view;
uniform mat4  projection;
uniform vec3  center;     // posição mundial da explosão
uniform vec3  camRight;
uniform vec3  camUp;
uniform float scale;
uniform vec2  uvOffset;   // canto inferior esquerdo do frame na spritesheet
uniform vec2  uvSize;     // tamanho de um frame na spritesheet (1/cols, 1/rows)

out vec2 vUV;

void main() {
    vec3 worldPos = center
        + camRight * aPos.x * scale
        + camUp    * aPos.y * scale;

    vUV = uvOffset + aTexCoord * uvSize;

    gl_Position = projection * view * vec4(worldPos, 1.0);
}
