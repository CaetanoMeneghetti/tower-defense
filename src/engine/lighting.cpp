#include "engine/lighting.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstdio>

void applyDirectionalLight(GLuint program,
                           const DirectionalLight& light,
                           const glm::vec3& viewPos) {
    glUniform3fv(glGetUniformLocation(program, "light.direction"), 1, glm::value_ptr(light.direction));
    glUniform3fv(glGetUniformLocation(program, "light.ambient"),   1, glm::value_ptr(light.ambient));
    glUniform3fv(glGetUniformLocation(program, "light.diffuse"),   1, glm::value_ptr(light.diffuse));
    glUniform3fv(glGetUniformLocation(program, "light.specular"),  1, glm::value_ptr(light.specular));
    glUniform3fv(glGetUniformLocation(program, "viewPos"),         1, glm::value_ptr(viewPos));
}

void applyPointLights(GLuint program, const std::vector<PointLight>& lights) {
    const int count = std::min(static_cast<int>(lights.size()), kMaxPointLights);
    glUniform1i(glGetUniformLocation(program, "numPointLights"), count);

    char namePos[64];
    char nameCol[64];
    for (int i = 0; i < count; ++i) {
        std::snprintf(namePos, sizeof(namePos), "pointLights[%d].position", i);
        std::snprintf(nameCol, sizeof(nameCol), "pointLights[%d].color", i);
        glUniform3fv(glGetUniformLocation(program, namePos), 1, glm::value_ptr(lights[i].position));
        glUniform3fv(glGetUniformLocation(program, nameCol), 1, glm::value_ptr(lights[i].color));
    }
}
