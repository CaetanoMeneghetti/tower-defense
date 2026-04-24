#pragma once

#include "math/matrix.h"
#include "math/vector.h"

class Camera {
 private:
  Vector<3> position_;
  Vector<3> view_;
  Vector<3> up_;

  float fov_;
  float aspect_;
  float near_;
  float far_;

 public:
  Camera();

  void setPosition(const Vector<3> &pos);
  void setLookAt(const Vector<3> &pos, const Vector<3> &target);
  void setPerspective(float fov, float aspect, float nearPlane, float farPlane);
  void setFPS(const Vector<3> &pos, float yaw, float pitch);

  Matrix<4, 4> getViewMatrix() const;
  Matrix<4, 4> getProjectionMatrix() const;
};
