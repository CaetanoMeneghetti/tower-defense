#include "input/input_handler.h"

#include <glad/glad.h>

#include "game/game_constants.h"
#include "math/opengl_utils.h"
#include "math/vector_ops.h"

AppState &stateFromWindow(GLFWwindow *window) {
  return *static_cast<AppState *>(glfwGetWindowUserPointer(window));
}

// =============================================================================
// CALLBACKS
// =============================================================================
namespace {

void mouseCallback(GLFWwindow *window, double xPosIn, double yPosIn) {
  AppState &s = stateFromWindow(window);
  if (!s.useFreeCamera) {
    return;
  }

  const float xPos = static_cast<float>(xPosIn);
  const float yPos = static_cast<float>(yPosIn);

  // Primeira leitura: descarta lixo da posição inicial
  if (s.firstMouse) {
    s.lastX = xPos;
    s.lastY = yPos;
    s.firstMouse = false;
  }

  float xoffset = xPos - s.lastX;
  float yoffset = s.lastY - yPos;  // Invertido: tela cresce para baixo
  s.lastX = xPos;
  s.lastY = yPos;

  xoffset *= game_constants::kMouseSensitivity;
  yoffset *= game_constants::kMouseSensitivity;

  s.yaw += xoffset;
  s.pitch += yoffset;

  if (s.pitch > game_constants::kMaxPitchRad) s.pitch = game_constants::kMaxPitchRad;
  if (s.pitch < -game_constants::kMaxPitchRad) s.pitch = -game_constants::kMaxPitchRad;
}

void scrollCallback(GLFWwindow *window, double /*xoffset*/, double yoffset) {
  AppState &s = stateFromWindow(window);
  if (s.useFreeCamera) {
    return;
  }
  s.orbitRadius -= static_cast<float>(yoffset);
  if (s.orbitRadius < game_constants::kMinOrbitalRadius) {
    s.orbitRadius = game_constants::kMinOrbitalRadius;
  }
  if (s.orbitRadius > game_constants::kMaxOrbitalRadius) {
    s.orbitRadius = game_constants::kMaxOrbitalRadius;
  }
}

void framebufferSizeCallback(GLFWwindow *window, int width, int height) {
  AppState &s = stateFromWindow(window);
  s.fbWidth = width;
  s.fbHeight = height;
  glViewport(0, 0, width, height);
}

}  // namespace

void registerInputCallbacks(GLFWwindow *window) {
  glfwSetCursorPosCallback(window, mouseCallback);
  glfwSetScrollCallback(window, scrollCallback);
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
}

// =============================================================================
// INPUT POR FRAME
// =============================================================================

void processInput(GLFWwindow *window, Vector<3> &cameraPosition, float deltaTime) {
  using namespace game_constants;

  AppState &s = stateFromWindow(window);

  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, true);
  }

  if (s.useFreeCamera) {
    const float speed = kFreeCameraSpeed * deltaTime;
    Vector<3> forward = directionFromYawPitch(s.yaw, s.pitch);
    Vector<3> up{0.0f, 1.0f, 0.0f};
    Vector<3> right = cross(forward, up);
    right.normalize();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPosition += forward * speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPosition -= forward * speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPosition -= right * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPosition += right * speed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) cameraPosition[1] += speed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) cameraPosition[1] -= speed;
  } else {
    const float angularSpeed = kOrbitalAngularSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) s.orbitYaw -= angularSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) s.orbitYaw += angularSpeed;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) s.orbitPitch += angularSpeed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) s.orbitPitch -= angularSpeed;

    if (s.orbitPitch > kMaxPitchRad) s.orbitPitch = kMaxPitchRad;
    if (s.orbitPitch < -kMaxPitchRad) s.orbitPitch = -kMaxPitchRad;
  }

  // Toggle de câmera em C
  if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
    if (!s.cPressed) {
      s.useFreeCamera = !s.useFreeCamera;
      s.cPressed = true;
      if (s.useFreeCamera) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        s.firstMouse = true;
      } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      }
    }
  } else {
    s.cPressed = false;
  }

  // Toggle da curva em T
  if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
    if (!s.tPressed) {
      s.showCurve = !s.showCurve;
      s.tPressed = true;
    }
  } else {
    s.tPressed = false;
  }
}
