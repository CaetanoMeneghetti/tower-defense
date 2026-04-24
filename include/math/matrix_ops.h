#pragma once

#include "math/matrix.h"

// R: linhas (rows) de A; C: colunas (columns) de A e linhas de B; K: colunas de B
// Esse operador permite realizar a multiplicação de matrizes apenas com "*"
template <size_t R, size_t K, size_t C>
Matrix<R, C> operator*(const Matrix<R, K> &a, const Matrix<K, C> &b) {
  Matrix<R, C> result;

  for (size_t i = 0; i < R; ++i) {
    for (size_t j = 0; j < C; ++j) {
      float sum = 0.0f;

      for (size_t k = 0; k < K; ++k) {
        sum += a(i, k) * b(k, j);
      }

      result(i, j) = sum;
    }
  }

  return result;
}
