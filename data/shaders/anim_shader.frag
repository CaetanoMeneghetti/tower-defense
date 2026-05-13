#version 330 core

out vec4 fragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D tex;
uniform vec3 viewPos;

// Luz direcional (sol/lua)
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight light;

const float SHININESS = 32.0;

vec3 calcDirLight(vec3 norm, vec3 viewDir, vec3 matColor) {
    vec3 lDir     = normalize(light.direction);
    vec3 ambient  = light.ambient * matColor;
    float diff    = max(dot(norm, lDir), 0.0);
    vec3 diffuse  = light.diffuse * diff * matColor;
    vec3 halfDir  = normalize(lDir + viewDir);
    float spec    = pow(max(dot(norm, halfDir), 0.0), SHININESS);
    vec3 specular = light.specular * spec;
    return ambient + diffuse + specular;
}

void main() {
    vec4 texColor = texture(tex, TexCoords);
    if (texColor.a < 0.1) discard;

    vec3 norm    = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    fragColor = vec4(calcDirLight(norm, viewDir, texColor.rgb), texColor.a);
}
