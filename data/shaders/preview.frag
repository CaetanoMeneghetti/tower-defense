#version 330 core
out vec4 FragColor;


uniform vec4 previewColor;

void main() {
    FragColor = previewColor; 
}