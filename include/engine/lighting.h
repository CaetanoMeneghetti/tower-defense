#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

// Luz direcional única (lua / sol)
struct DirectionalLight {
    glm::vec3 direction;  // world-space, aponta em direção à fonte (normalizado)
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

// Point light pontual com atenuação física (1 / (Kc + Kl*d + Kq*d^2))
// usada para as lanternas. Coeficientes Kc/Kl/Kq são constantes nos shaders.
struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
};

// Limite usado nos shaders. Manter sincronizado com MAX_POINT_LIGHTS no GLSL.
constexpr int kMaxPointLights = 20;

// Sobe todos os uniforms de iluminação pro programa ativo.
// O shader precisa de "struct DirLight { ... } uniform DirLight light;" e "uniform vec3 viewPos;".
void applyDirectionalLight(GLuint program,
                           const DirectionalLight& light,
                           const glm::vec3& viewPos);

// O shader precisa de:
//   struct PointLight { vec3 position; vec3 color; };
//   uniform int numPointLights;
//   uniform PointLight pointLights[MAX_POINT_LIGHTS];
void applyPointLights(GLuint program,
                      const std::vector<PointLight>& lights);
