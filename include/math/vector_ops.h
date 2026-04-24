#pragma once

#include "math/vector.h"

template <size_t N>
float dot(const Vector<N> &a, const Vector<N> &b) {
  float sum = 0.0f;
  for (size_t i = 0; i < N; ++i) {
    sum += a[i] * b[i];
  }
  return sum;
}

template <size_t N>
Vector<N> cross(const Vector<N> &a, const Vector<N> &b) {
  static_assert(N == 3, "Produto vetorial só está definido para vetores 3D");

  Vector<N> result;

  result[0] = a[1] * b[2] - a[2] * b[1];
  result[1] = a[2] * b[0] - a[0] * b[2];
  result[2] = a[0] * b[1] - a[1] * b[0];

  return result;
}

template <size_t N>
float distance(const Vector<N> &a, const Vector<N> &b) {
  Vector<N> diff;

  for (size_t i = 0; i < N; ++i) {
    diff[i] = a[i] - b[i];
  }

  return diff.getNorm();
}

template <size_t N>
Vector<N> operator+(const Vector<N> &a, const Vector<N> &b) {
  Vector<N> result = a;
  result += b;
  return result;
}

template <size_t N>
Vector<N> operator-(const Vector<N> &a, const Vector<N> &b) {
  Vector<N> result = a;
  result -= b;
  return result;
}

template <size_t N>
Vector<N> operator*(const Vector<N> &v, float scalar) {
  Vector<N> result = v;
  result *= scalar;
  return result;
}

template <size_t N>
Vector<N> operator*(float scalar, const Vector<N> &v) {
  return v * scalar;
}

template <size_t N>
Vector<N> operator/(const Vector<N> &v, float scalar) {
  Vector<N> result = v;
  result *= 1.0f / scalar;
  return result;
}
