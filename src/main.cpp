#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "engine/camera.h"
#include "engine/catmull_rom.h"
#include "engine/mesh.h"
#include "engine/parser.h"
#include "engine/shader_utils.h"
#include "math/constants.h"
#include "math/matrix.h"
#include "math/opengl_utils.h"
#include "math/transforms.h"
#include "math/vector.h"
#include "math/vector_ops.h"
#include "stb_image.h"
#include "world/path_generator.h"

namespace {

  constexpr int kWindowWidth = 1920;
  constexpr int kWindowHeight = 1080;
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
  constexpr char kVertexShaderPath[] = "data/shaders/grass.vert";
  constexpr char kFragmentShaderPath[] = "data/shaders/grass.frag";

  struct AppConfig {
    bool useFreeCamera = false;
    bool cPressed = false;
    bool showCurve = false;
    bool tPressed = false;
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

  // Toggle da curva
  if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
    if (!gAppConfig.tPressed) {
      gAppConfig.showCurve = !gAppConfig.showCurve;
      gAppConfig.tPressed = true;
    }
  } else {
    gAppConfig.tPressed = false;
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

  // [TEMPORÁRIO] Gera um plano simples no XZ para servir de chão texturizado com grama
  // clang-format off
  // Considerar criar um struct Vertex de 5 floats
  std::vector<float> grassVertices = {
    //  X      Y      Z      U      V
    // PRIMEIRO TRIÂNGULO
    -20.0f,  0.0f, -20.0f,  0.0f,  20.0f, // Trás Esquerda
     20.0f,  0.0f, -20.0f,  20.0f, 20.0f, // Trás Direita
    -20.0f,  0.0f,  20.0f,  0.0f,  0.0f,  // Frente Esquerda

    // SEGUNDO TRIÂNGULO
     20.0f,  0.0f, -20.0f, 20.0f, 20.0f,  // Trás Direita
     20.0f,  0.0f,  20.0f, 20.0f,  0.0f,  // Frente Direita
    -20.0f,  0.0f,  20.0f,  0.0f,  0.0f   // Frente Esquerda
  };
  // clang-format on

  unsigned int grassVAO, grassVBO;
  glGenVertexArrays(1, &grassVAO);
  glGenBuffers(1, &grassVBO);
  glBindVertexArray(grassVAO);
  glBindBuffer(GL_ARRAY_BUFFER, grassVBO);
  glBufferData(GL_ARRAY_BUFFER,
               grassVertices.size() * sizeof(float),
               grassVertices.data(),
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // Instancia a curva de teste
  std::vector<Point> controlPoints = {
      {-10.0000f, -4.1615f}, {-8.9474f, -1.9551f}, {-7.8947f, -0.0643f},
      {-6.8421f, 1.3658f},   {-5.7895f, 2.3234f},  {-4.7368f, 2.7654f},
      {-3.6842f, 2.7286f},   {-2.6316f, 2.2750f},  {-1.5789f, 1.5009f},
      {-0.5263f, 0.5234f},   {0.5263f, -0.5234f},  {1.5789f, -1.5009f},
      {2.6316f, -2.2750f},   {3.6842f, -2.7286f},  {4.7368f, -2.7654f},
      {5.7895f, -2.3234f},   {6.8421f, -1.3658f},  {7.8947f, 0.0643f},
      {8.9474f, 1.9551f},    {10.0000f, 4.1615f}};

  std::vector<Point> curvePoints = generateCatmullRomVertices(controlPoints);
  std::vector<float> curveVertices;
  for (const Point &p : curvePoints) {
    curveVertices.push_back(p.x);
    curveVertices.push_back(0.1f);  // Um pouco acima do chao para nao dar z-fighting
    curveVertices.push_back(p.y);
  }

  Mesh pathMesh = generatePathMesh(curvePoints, 2.0f);

  unsigned int curveVAO, curveVBO;
  glGenVertexArrays(1, &curveVAO);
  glGenBuffers(1, &curveVBO);
  glBindVertexArray(curveVAO);
  glBindBuffer(GL_ARRAY_BUFFER, curveVBO);
  glBufferData(GL_ARRAY_BUFFER,
               curveVertices.size() * sizeof(float),
               curveVertices.data(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  unsigned int objShaderProgram =
      createShaderProgram("data/shaders/shader.vert", "data/shaders/shader.frag");
  if (objShaderProgram == 0) {
    return 1;
  }

  unsigned int pathShaderProgram =
      createShaderProgram("data/shaders/path.vert", "data/shaders/path.frag");
  if (pathShaderProgram == 0) {
    return 1;
  }

  std::vector<Vertex> objVertices;

  if (!Parser("data/models/test.obj", objVertices)) {
    std::cout << "ERRO: Nao encontrou data/models/test.obj" << std::endl;
    return 1;
  }
  Mesh test(objVertices);

  Camera cam;
  Vector<3> cameraPosition{0.0f, 2.0f, 5.0f};

  // Carrega e cria a textura da grama
  unsigned int grassTexture;
  glGenTextures(1, &grassTexture);
  glBindTexture(GL_TEXTURE_2D, grassTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  int grassWidth, grassHeight, grassChannels;
  unsigned char *grassData = stbi_load(
      "data/textures/grass_color.png", &grassWidth, &grassHeight, &grassChannels, 0);

  if (grassData) {
    GLenum format = GL_RGB;
    if (grassChannels == 4) format = GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 format,
                 grassWidth,
                 grassHeight,
                 0,
                 format,
                 GL_UNSIGNED_BYTE,
                 grassData);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    std::cout << "ERRO: Falha ao carregar a textura data/textures/grass.png" << std::endl;
  }
  stbi_image_free(grassData);

  // Carrega e cria a textura da terra
  unsigned int dirtTexture;
  glGenTextures(1, &dirtTexture);
  glBindTexture(GL_TEXTURE_2D, dirtTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  int width, height, nrChannels;
  unsigned char *dirtData =
      stbi_load("data/textures/dirt_color.png", &width, &height, &nrChannels, 0);

  if (dirtData) {
    GLenum format = GL_RGB;
    if (nrChannels == 4) format = GL_RGBA;
    glTexImage2D(
        GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, dirtData);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    std::cout << "ERRO: Falha ao carregar a textura data/textures/dirt_color.png"
              << std::endl;
  }
  stbi_image_free(dirtData);

  unsigned int noiseTexture;
  glGenTextures(1, &noiseTexture);
  glBindTexture(GL_TEXTURE_2D, noiseTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  int noiseWidth, noiseHeight, noiseChannels;
  unsigned char *noiseData = stbi_load(
      "data/textures/perlin_noise.jpg", &noiseWidth, &noiseHeight, &noiseChannels, 3);

  if (noiseData) {
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 noiseWidth,
                 noiseHeight,
                 0,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 noiseData);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    std::cout << "ERRO: Falha ao carregar a textura data/textures/perlin_noise.jpg"
              << std::endl;
  }
  stbi_image_free(noiseData);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

  glUseProgram(pathShaderProgram);
  glUniform1i(glGetUniformLocation(pathShaderProgram, "dirt"), 0);
  glUniform1i(glGetUniformLocation(pathShaderProgram, "noise"), 1);

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

    // =====================================================================
    // DESENHA AS ENTIDADES
    // =====================================================================
    glUseProgram(objShaderProgram);
    unsigned int objViewLoc = glGetUniformLocation(objShaderProgram, "view");
    unsigned int objProjLoc = glGetUniformLocation(objShaderProgram, "projection");
    unsigned int objModelLoc = glGetUniformLocation(objShaderProgram, "model");

    glUniformMatrix4fv(objViewLoc, 1, GL_FALSE, glView.data());
    glUniformMatrix4fv(objProjLoc, 1, GL_FALSE, glProj.data());

    Matrix<4, 4> identity = Matrix<4, 4>::identity();
    glUniformMatrix4fv(objModelLoc, 1, GL_FALSE, identity.getData());

    test.Draw();

    // =====================================================================
    // DESENHA A GRAMA E A CURVA
    // =====================================================================
    glUseProgram(shaderProgram);

    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glView.data());
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glProj.data());

    glUniform1i(glGetUniformLocation(shaderProgram, "grass"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "noise"), 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, grassTexture);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);

    glUniform1i(glGetUniformLocation(shaderProgram, "isGrass"), 1);

    glBindVertexArray(grassVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    if (gAppConfig.showCurve) {
      glUniform1i(glGetUniformLocation(shaderProgram, "isGrass"), 0);
      glBindVertexArray(curveVAO);
      glDrawArrays(GL_LINE_STRIP, 0, curveVertices.size() / 3);
    }

    // =====================================================================
    // DESENHA O CAMINHO DE TERRA
    // =====================================================================
    glUseProgram(pathShaderProgram);

    unsigned int pathViewLoc = glGetUniformLocation(pathShaderProgram, "view");
    unsigned int pathProjLoc = glGetUniformLocation(pathShaderProgram, "projection");
    unsigned int pathModelLoc = glGetUniformLocation(pathShaderProgram, "model");

    glUniformMatrix4fv(pathViewLoc, 1, GL_FALSE, glView.data());
    glUniformMatrix4fv(pathProjLoc, 1, GL_FALSE, glProj.data());
    glUniformMatrix4fv(pathModelLoc, 1, GL_FALSE, identity.getData());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, dirtTexture);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);

    pathMesh.Draw();

    glDisable(GL_BLEND);

    // =====================================================================
    // ATUALIZA A TELA
    // =====================================================================
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // =====================================================================
  // LIMPEZA DE MEMÓRIA
  // =====================================================================
  glDeleteProgram(objShaderProgram);
  glDeleteProgram(pathShaderProgram);
  glDeleteProgram(shaderProgram);
  glDeleteBuffers(1, &grassVBO);
  glDeleteVertexArrays(1, &grassVAO);
  glfwDestroyWindow(window);
  glfwTerminate();
}
