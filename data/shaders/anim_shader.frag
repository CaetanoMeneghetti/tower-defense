#version 330 core

out vec4 fragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D tex;

void main() {

    vec4 texColor = texture(tex, TexCoords);
    if(texColor.a < 0.1) discard;
    
    fragColor = texColor;
}