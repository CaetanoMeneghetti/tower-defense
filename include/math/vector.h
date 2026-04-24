#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <numeric>

#include "math/constants.h"

// Uso de template permite criar uma única classe Vector que generalize para
// Vec2, Vec3, Vec4, ...
// Vector é usado para pontos também (com w = 1)
template <size_t N>
class Vector {
 private:
  std::array<float, N> data_;

 public:
  Vector() = default;
  explicit Vector(const std::array<float, N> &values);
  Vector(std::initializer_list<float> values);

  Vector<N> &operator+=(const Vector<N> &v);
  Vector<N> &operator-=(const Vector<N> &v);
  Vector<N> &operator*=(float scalar);

  // Permite acessar um elemento de 'data_' sem precisar de um método 'getData()'
  float &operator[](size_t i);
  const float &operator[](size_t i) const;

  float getNorm() const;  // L2

  void normalize();
};

template <size_t N>
Vector<N>::Vector(const std::array<float, N> &values) : data_(values) {}

template <size_t N>
Vector<N>::Vector(std::initializer_list<float> values) {
  size_t i = 0;
  for (float value : values) {
    if (i >= N) {
      break;
    }
    data_[i++] = value;
  }
  for (; i < N; ++i) {
    data_[i] = 0.0f;
  }
}

template <size_t N>
Vector<N> &Vector<N>::operator+=(const Vector<N> &v) {
  for (size_t i = 0; i < N; ++i) {
    data_[i] += v[i];
  }
  return *this;
}

template <size_t N>
Vector<N> &Vector<N>::operator-=(const Vector<N> &v) {
  for (size_t i = 0; i < N; ++i) {
    data_[i] -= v[i];
  }
  return *this;
}

template <size_t N>
Vector<N> &Vector<N>::operator*=(float scalar) {
  for (size_t i = 0; i < N; ++i) {
    data_[i] *= scalar;
  }
  return *this;
}

template <size_t N>
float &Vector<N>::operator[](size_t i) {
  return data_[i];
}

template <size_t N>
const float &Vector<N>::operator[](size_t i) const {
  return data_[i];
}

template <size_t N>
float Vector<N>::getNorm() const {
  float norm = 0.0f;
  for (size_t i = 0; i < N; i++) {
    norm += data_[i] * data_[i];
  }
  norm = std::sqrt(norm);
  return norm;
}

template <size_t N>
void Vector<N>::normalize() {
  float n = getNorm();

  // Evita divisão por zero (aproximação com epsilon)
  if (n <= math_constants::kEpsilon) return;

  for (size_t i = 0; i < N; ++i) {
    data_[i] /= n;
  }
}
