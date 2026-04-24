#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "engine/camera.h"
#include "engine/shader_utils.h"
#include "math/constants.h"
#include "math/matrix.h"
#include "math/opengl_utils.h"
#include "math/vector.h"
#include "math/vector_ops.h"

namespace {

  constexpr int kWindowWidth = 800;
  constexpr int kWindowHeight = 600;
  constexpr float kMouseSensitivity = 0.005f;
  constexpr float kFreeCameraSpeed = 5.0f;
  constexpr float kOrbitalAngularSpeed = 2.0f;
  constexpr float kMaxPitchRad = 89.0f * math_constants::kDegToRad;
  constexpr float kMinOrbitalRadius = 1.0f;
  constexpr float kMaxOrbitalRadius = 50.0f;
  constexpr float kFovDegrees = 45.0f;
  constexpr float kNearPlane = 0.1f;
  constexpr float kFarPlane = 100.0f;
  constexpr char kWindowTitle[] = "Tower Defense";
  constexpr char kVertexShaderPath[] = "data/shaders/grid.vert";
  constexpr char kFragmentShaderPath[] = "data/shaders/grid.frag";

  struct AppConfig {
    bool useFreeCamera = false;
    bool cPressed = false;
  };

  struct MouseState {
    float yaw = -math_constants::kHalfPi;
    float pitch = 0.0f;
    float lastX = kWindowWidth / 2.0f;
    float lastY = kWindowHeight / 2.0f;
    bool firstMouse = true;
  };

  struct OrbitalState {
    float yaw = 0.0f;
    float pitch = math_constants::kPi / 6.0f;
    float radius = 10.0f;
  };

  // Usamos o prefixo 'g' para denotar valores globais
  AppConfig gAppConfig;
  MouseState gMouse;
  OrbitalState gOrbital;

}  // namespace

std::vector<float> buildGridLines(int minCoord, int maxCoord) {
  std::vector<float> grid;

  const float minF = static_cast<float>(minCoord);
  const float maxF = static_cast<float>(maxCoord);

  // Cada valor gera 2 linhas (eixos X e Z)
  for (int i = minCoord; i <= maxCoord; ++i) {
    float p = static_cast<float>(i);

    // Linha paralela ao eixo Z
    // Isso está colocando os pontos (p, 0, minF) e (p, 0, maxF) no final de grid
    grid.insert(grid.end(), {p, 0.0f, minF, p, 0.0f, maxF});
    // Linha paralela ao eixo X
    // Isso está colocando os pontos (minF, 0, p) e (maxF, 0, p) no final de grid
    grid.insert(grid.end(), {minF, 0.0f, p, maxF, 0.0f, p});
  }

  return grid;
}

void updateOrbitalCameraPosition(Vector<3> &cameraPos) {
  const float radius = gOrbital.radius;
  const float pitch = gOrbital.pitch;
  const float yaw = gOrbital.yaw;

  cameraPos[0] = radius * std::cos(pitch) * std::sin(yaw);
  cameraPos[1] = radius * std::sin(pitch);
  cameraPos[2] = radius * std::cos(pitch) * std::cos(yaw);
}

// A assinatura exige o formato (GLFWwindow *window, double xPosIn, double
// yPosIn), mesmo que não usemos alguns desses parâmetros
void mouseCallback(GLFWwindow *window, double xPosIn, double yPosIn) {
  // Se a câmera livre não está ativada, ignora movimento do mouse
  if (!gAppConfig.useFreeCamera) {
    return;
  }

  const float xPos = static_cast<float>(xPosIn);
  const float yPos = static_cast<float>(yPosIn);

  // Carrega variáveis na primeira leitura do mouse, para tirar lixo das variáveis
  if (gMouse.firstMouse) {
    gMouse.lastX = xPos;
    gMouse.lastY = yPos;
    gMouse.firstMouse = false;
  }

  // Calcula deslocamento do mouse desde o último frame e atualiza última posição do mouse
  float xoffset = xPos - gMouse.lastX;
  float yoffset = gMouse.lastY - yPos;  // Invertido porque a tela cresce para baixo
  gMouse.lastX = xPos;
  gMouse.lastY = yPos;

  xoffset *= kMouseSensitivity;
  yoffset *= kMouseSensitivity;

  gMouse.yaw += xoffset;
  gMouse.pitch += yoffset;

  // Limita o pitch em [kMaxPitchRad, -kMaxPitchRad] para que a câmera não vire de cabeça
  // para baixo
  if (gMouse.pitch > kMaxPitchRad) {
    gMouse.pitch = kMaxPitchRad;
  }
  if (gMouse.pitch < -kMaxPitchRad) {
    gMouse.pitch = -kMaxPitchRad;
  }
}

// A assinatura exige o formato (GLFWwindow *window, double xoffset, double
// yoffset), mesmo que não usemos alguns desses parâmetros
void scrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
  if (!gAppConfig.useFreeCamera) {
    // Muda o raio da esfera conforme o scroll
    gOrbital.radius -= static_cast<float>(yoffset);

    // Limita o entre [kMinOrbitalRadius, kMaxOrbitalRadius]
    if (gOrbital.radius < kMinOrbitalRadius) {
      gOrbital.radius = kMinOrbitalRadius;
    }
    if (gOrbital.radius > kMaxOrbitalRadius) {
      gOrbital.radius = kMaxOrbitalRadius;
    }
  }
}

void processInput(GLFWwindow *window, Vector<3> &cameraPosition, float deltaTime) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, true);
  }

  // Câmera livre
  if (gAppConfig.useFreeCamera) {
    const float speed = kFreeCameraSpeed * deltaTime;

    Vector<3> forward = directionFromYawPitch(gMouse.yaw, gMouse.pitch);
    Vector<3> up{0.0f, 1.0f, 0.0f};
    Vector<3> right = cross(forward, up);
    right.normalize();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
      cameraPosition += forward * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
      cameraPosition -= forward * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
      cameraPosition -= right * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
      cameraPosition += right * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
      cameraPosition[1] += speed;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
      cameraPosition[1] -= speed;
    }
    // Câmera look-at
  } else {
    const float angularSpeed = kOrbitalAngularSpeed * deltaTime;

    // yaw anda horizontalmente na esfera
    // pitch anda verticalmente na esfera
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
      gOrbital.yaw -= angularSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
      gOrbital.yaw += angularSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
      gOrbital.pitch += angularSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
      gOrbital.pitch -= angularSpeed;
    }

    // Limita o pitch em [kMaxPitchRad, -kMaxPitchRad] para que a câmera não vire de
    // cabeça para baixo
    if (gOrbital.pitch > kMaxPitchRad) {
      gOrbital.pitch = kMaxPitchRad;
    }
    if (gOrbital.pitch < -kMaxPitchRad) {
      gOrbital.pitch = -kMaxPitchRad;
    }
  }

  // Mudança de câmera
  if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
    if (!gAppConfig.cPressed) {
      gAppConfig.useFreeCamera = !gAppConfig.useFreeCamera;
      gAppConfig.cPressed = true;

      // Desabilita cursor na câmera livre, pois o cursor é sempre o centro da câmera
      // Ativa o cursos na câmera look-at, pois os comandos são pelo teclado
      if (gAppConfig.useFreeCamera) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        gMouse.firstMouse = true;
      } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      }
    }
  } else {
    gAppConfig.cPressed = false;
  }
}

int main() {
  if (!glfwInit()) {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window =
      glfwCreateWindow(kWindowWidth, kWindowHeight, kWindowTitle, nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  glEnable(GL_DEPTH_TEST);

  glfwSetCursorPosCallback(window, mouseCallback);
  glfwSetScrollCallback(window, scrollCallback);

  unsigned int shaderProgram =
      createShaderProgram(kVertexShaderPath, kFragmentShaderPath);
  if (shaderProgram == 0) {
    glfwTerminate();
    return 1;
  }
  glUseProgram(shaderProgram);

  // [TEMPORÁRIO] Gera uma grade simples no plano XZ para servir de chão
  const std::vector<float> grid = buildGridLines(-20, 20);

  unsigned int VAO, VBO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, grid.size() * sizeof(float), grid.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  Camera cam;
  Vector<3> cameraPosition{0.0f, 2.0f, 5.0f};

  float lastFrame = 0.0f;

  while (!glfwWindowShouldClose(window)) {
    const float currentFrame = glfwGetTime();
    const float deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    processInput(window, cameraPosition, deltaTime);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    cam.setPerspective(
        kFovDegrees * math_constants::kDegToRad,
        static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight),
        kNearPlane,
        kFarPlane);

    if (gAppConfig.useFreeCamera) {
      cam.setFPS(cameraPosition, gMouse.yaw, gMouse.pitch);
    } else {
      updateOrbitalCameraPosition(cameraPosition);

      Vector<3> lookTarget{0.0f, 0.0f, 0.0f};
      cam.setLookAt(cameraPosition, lookTarget);
    }

    Matrix<4, 4> view = cam.getViewMatrix();
    Matrix<4, 4> projection = cam.getProjectionMatrix();

    auto glView = toOpenGLMatrix(view);
    auto glProj = toOpenGLMatrix(projection);

    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glView.data());
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glProj.data());

    glDrawArrays(GL_LINES, 0, grid.size() / 3);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glDeleteProgram(shaderProgram);
  glDeleteBuffers(1, &VBO);
  glDeleteVertexArrays(1, &VAO);
  glfwDestroyWindow(window);
  glfwTerminate();
}
