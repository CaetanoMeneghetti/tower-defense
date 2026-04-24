#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>

// R: linhas (rows); C: colunas (columns)
template <size_t R, size_t C>
class Matrix {
 private:
  std::array<float, R * C> data_;

 public:
  // Permite acessar um elemento de 'data_' sem precisar escrever 'getData()'
  float &operator()(size_t i, size_t j);
  const float &operator()(size_t i, size_t j) const;

  static Matrix<R, C> identity();

  size_t getNumRows() const;
  size_t getNumColumns() const;
};

template <size_t R, size_t C>
float &Matrix<R, C>::operator()(size_t i, size_t j) {
  return data_[i * C + j];
}

template <size_t R, size_t C>
const float &Matrix<R, C>::operator()(size_t i, size_t j) const {
  return data_[i * C + j];
}

template <size_t R, size_t C>
size_t Matrix<R, C>::getNumRows() const {
  return R;
}

template <size_t R, size_t C>
size_t Matrix<R, C>::getNumColumns() const {
  return C;
}

template <size_t R, size_t C>
Matrix<R, C> Matrix<R, C>::identity() {
  static_assert(R == C, "Indentidade só existe para matrizes quadradas");

  Matrix<R, C> identityMatrix;

  for (size_t i = 0; i < R; ++i) {
    for (size_t j = 0; j < C; ++j) {
      identityMatrix(i, j) = (i == j);
    }
  }

  return identityMatrix;
}
