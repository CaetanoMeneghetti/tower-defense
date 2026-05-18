#include "input/camera_controller.h"

#include <cmath>

#include "math/constants.h"
#include "math/matrix.h"
#include "render/render_constants.h"

void updateOrbitalCameraPosition(const AppState &s, Vector<3> &cameraPos) {
  cameraPos[0] = s.orbitRadius * std::cos(s.orbitPitch) * std::sin(s.orbitYaw);
  cameraPos[1] = s.orbitRadius * std::sin(s.orbitPitch);
  cameraPos[2] = s.orbitRadius * std::cos(s.orbitPitch) * std::cos(s.orbitYaw);
}

Vector<3> getMouseGroundPosition(GLFWwindow *window,
                                 const Camera &cam,
                                 const Vector<3> &camPos,
                                 int width,
                                 int height) {
  double mouseX, mouseY;
  glfwGetCursorPos(window, &mouseX, &mouseY);

  // 1. Converte a posição do mouse para coordenadas normalizadas (NDC) [-1, 1]
  float x = (2.0f * static_cast<float>(mouseX)) / static_cast<float>(width) - 1.0f;
  float y = 1.0f - (2.0f * static_cast<float>(mouseY)) / static_cast<float>(height);

  // 2. Extrair os eixos (Right, Up, Forward) diretamente da View Matrix.
  // A rotação da câmera fica transposta no canto superior esquerdo 3x3.
  Matrix<4, 4> V = cam.getViewMatrix();
  Vector<3> right{V(0, 0), V(0, 1), V(0, 2)};
  Vector<3> up{V(1, 0), V(1, 1), V(1, 2)};
  Vector<3> backward{V(2, 0), V(2, 1), V(2, 2)};

  // 3. Parâmetros de projeção (devem casar com os usados em setPerspective)
  float aspect = static_cast<float>(width) / static_cast<float>(height);
  float tanHalfFov =
      std::tan((render_constants::kFovDegrees * math_constants::kDegToRad) / 2.0f);

  // 4. Direção do raio no espaço da câmera (Z = -1, plano de imagem)
  float vx = x * aspect * tanHalfFov;
  float vy = y * tanHalfFov;
  float vz = -1.0f;

  // 5. View → World multiplicando pelos vetores base
  Vector<3> rayWorld;
  rayWorld[0] = vx * right[0] + vy * up[0] + vz * backward[0];
  rayWorld[1] = vx * right[1] + vy * up[1] + vz * backward[1];
  rayWorld[2] = vx * right[2] + vy * up[2] + vz * backward[2];

  float length = std::sqrt(rayWorld[0] * rayWorld[0] +
                           rayWorld[1] * rayWorld[1] +
                           rayWorld[2] * rayWorld[2]);
  rayWorld[0] /= length;
  rayWorld[1] /= length;
  rayWorld[2] /= length;

  // 6. Interseção com o plano Y = 0
  if (rayWorld[1] >= 0.0f) {
    return Vector<3>{0.0f, 0.0f, 0.0f};
  }

  float t = -camPos[1] / rayWorld[1];

  Vector<3> result;
  result[0] = camPos[0] + rayWorld[0] * t;
  result[1] = 0.0f;
  result[2] = camPos[2] + rayWorld[2] * t;

  return result;
}
