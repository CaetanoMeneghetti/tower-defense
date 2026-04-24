#include "engine/camera.h"

#include <cmath>

#include "math/constants.h"
#include "math/matrix_ops.h"
#include "math/transforms.h"
#include "math/vector_ops.h"

Camera::Camera() {
  position_ = Vector<3>{0.0f, 0.0f, 0.0f};
  view_ = Vector<3>{0.0f, 0.0f, -1.0f};
  up_ = Vector<3>{0.0f, 1.0f, 0.0f};

  fov_ = math_constants::kQuarterPi;
  aspect_ = 4.0f / 3.0f;
  near_ = 0.1f;
  far_ = 100.0f;
}

void Camera::setLookAt(const Vector<3> &pos, const Vector<3> &target) {
  setPosition(pos);

  Vector<3> dir = target - pos;
  if (dir.getNorm() <= math_constants::kEpsilon) {
    return;
  }

  dir.normalize();
  view_ = dir;
}

void Camera::setFPS(const Vector<3> &pos, float yaw, float pitch) {
  setPosition(pos);

  Vector<3> dir{
      std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch)};
  dir.normalize();

  view_ = dir;
}

void Camera::setPosition(const Vector<3> &pos) {
  position_ = pos;
}

void Camera::setPerspective(float fov, float aspect, float near, float far) {
  fov_ = fov;
  aspect_ = aspect;
  near_ = near;
  far_ = far;
}

Matrix<4, 4> Camera::getProjectionMatrix() const {
  Matrix<4, 4> proj = Matrix<4, 4>::identity();

  float f = 1.0f / std::tan(fov_ / 2.0f);

  proj(0, 0) = f / aspect_;
  proj(1, 1) = f;
  proj(2, 2) = (near_ + far_) / (near_ - far_);
  proj(2, 3) = (2.0f * far_ * near_) / (near_ - far_);
  proj(3, 2) = -1.0f;
  proj(3, 3) = 0.0f;

  return proj;
}

Matrix<4, 4> Camera::getViewMatrix() const {
  Matrix<4, 4> view = Matrix<4, 4>::identity();

  Vector<3> forward = view_;
  forward.normalize();

  Vector<3> right = cross(forward, up_);
  right.normalize();

  Vector<3> vert = cross(right, forward);
  vert.normalize();

  view(0, 0) = right[0];
  view(0, 1) = right[1];
  view(0, 2) = right[2];
  view(0, 3) = -dot(right, position_);

  view(1, 0) = vert[0];
  view(1, 1) = vert[1];
  view(1, 2) = vert[2];
  view(1, 3) = -dot(vert, position_);

  view(2, 0) = -forward[0];
  view(2, 1) = -forward[1];
  view(2, 2) = -forward[2];
  view(2, 3) = dot(forward, position_);

  return view;
}
