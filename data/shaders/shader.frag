#version 330 core

out vec4 fragColor;

in vec2 texCoord;  
in vec3 objNormal; 

uniform sampler2D tex; 

void main() {
    fragColor = texture(tex, texCoord);
}