#include "engine/lighting.h"
#include <glm/gtc/type_ptr.hpp>

void applyDirectionalLight(GLuint program,
                           const DirectionalLight& light,
                           const glm::vec3& viewPos) {
    glUniform3fv(glGetUniformLocation(program, "light.direction"), 1, glm::value_ptr(light.direction));
    glUniform3fv(glGetUniformLocation(program, "light.ambient"),   1, glm::value_ptr(light.ambient));
    glUniform3fv(glGetUniformLocation(program, "light.diffuse"),   1, glm::value_ptr(light.diffuse));
    glUniform3fv(glGetUniformLocation(program, "light.specular"),  1, glm::value_ptr(light.specular));
    glUniform3fv(glGetUniformLocation(program, "viewPos"),         1, glm::value_ptr(viewPos));
}
