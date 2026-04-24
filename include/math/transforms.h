#pragma once

#include "math/matrix.h"

template <size_t R, size_t C>
Matrix<R, C> translate(float tx, float ty, float tz) {
  static_assert(R == 4 && C == 4, "Translação só está definida para matrizes 4x4");

  Matrix<4, 4> transformMatrix = Matrix<4, 4>::identity();

  transformMatrix(0, 3) = tx;
  transformMatrix(1, 3) = ty;
  transformMatrix(2, 3) = tz;

  return transformMatrix;
}

template <size_t R, size_t C>
Matrix<R, C> rotateX(float angle) {
  static_assert(R == 4 && C == 4, "Rotação só está definida para matrizes 4x4");

  Matrix<4, 4> transformMatrix = Matrix<4, 4>::identity();

  float c = std::cos(angle);
  float s = std::sin(angle);

  transformMatrix(1, 1) = c;
  transformMatrix(1, 2) = -s;
  transformMatrix(2, 1) = s;
  transformMatrix(2, 2) = c;

  return transformMatrix;
}

template <size_t R, size_t C>
Matrix<R, C> rotateY(float angle) {
  static_assert(R == 4 && C == 4, "Rotação só está definida para matrizes 4x4");

  Matrix<4, 4> transformMatrix = Matrix<4, 4>::identity();

  float c = std::cos(angle);
  float s = std::sin(angle);

  transformMatrix(0, 0) = c;
  transformMatrix(0, 2) = s;
  transformMatrix(2, 0) = -s;
  transformMatrix(2, 2) = c;

  return transformMatrix;
}

template <size_t R, size_t C>
Matrix<R, C> rotateZ(float angle) {
  static_assert(R == 4 && C == 4, "Rotação só está definida para matrizes 4x4");

  Matrix<4, 4> transformMatrix = Matrix<4, 4>::identity();

  float c = std::cos(angle);
  float s = std::sin(angle);

  transformMatrix(0, 0) = c;
  transformMatrix(0, 1) = -s;
  transformMatrix(1, 0) = s;
  transformMatrix(1, 1) = c;

  return transformMatrix;
}

template <size_t R, size_t C>
Matrix<R, C> scale(float sx, float sy, float sz) {
  static_assert(R == 4 && C == 4, "Escalonamento só está definido para matrizes 4x4");

  Matrix<4, 4> transformMatrix = Matrix<4, 4>::identity();

  transformMatrix(0, 0) = sx;
  transformMatrix(1, 1) = sy;
  transformMatrix(2, 2) = sz;

  return transformMatrix;
}
