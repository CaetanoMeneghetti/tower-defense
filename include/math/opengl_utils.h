#pragma once

#include <array>
#include <cmath>

#include "math/matrix.h"
#include "math/vector.h"

// Converte uma matriz 4x4 para o formato que o OpenGL espera na memória
inline std::array<float, 16> toOpenGLMatrix(const Matrix<4, 4> &m) {
  std::array<float, 16> arr;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      arr[j * 4 + i] = m(i, j);
    }
  }
  return arr;
}

// Converte os ângulos de orientação (yaw e pitch) em um vetor de direção normalizado
inline Vector<3> directionFromYawPitch(float yawAngle, float pitchAngle) {
  Vector<3> dir{std::cos(yawAngle) * std::cos(pitchAngle),
                std::sin(pitchAngle),
                std::sin(yawAngle) * std::cos(pitchAngle)};
  dir.normalize();
  return dir;
}
