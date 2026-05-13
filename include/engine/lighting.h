#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

// Luz direcional única (lua / sol)
struct DirectionalLight {
    glm::vec3 direction;  // world-space, aponta em direção à fonte (normalizado)
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

// Sobe todos os uniforms de iluminação pro programa ativo.
// O shader precisa de "struct DirLight { ... } uniform DirLight light;" e "uniform vec3 viewPos;".
void applyDirectionalLight(GLuint program,
                           const DirectionalLight& light,
                           const glm::vec3& viewPos);
