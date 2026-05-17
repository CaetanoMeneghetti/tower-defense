#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>
#include <ctime>
#include <limits>
#include <memory>
#include <random>

#include "engine/app_state.h"
#include "engine/hud.h"
#include "engine/camera.h"
#include "engine/catmull_rom.h"
#include "engine/lighting.h"
#include "engine/mesh.h"
#include "engine/parser.h"
#include "engine/shader_utils.h"
#include "math/constants.h"
#include "math/matrix.h"
#include "math/matrix_ops.h"
#include "math/opengl_utils.h"
#include "math/transforms.h"
#include "math/vector.h"
#include "math/vector_ops.h"
#include "stb_image.h"
#include "world/path_generator.h"
#include "engine/animatedmodel.h"
#include "engine/gameobject.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>


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
  constexpr char kWindowTitle[] = "1346AD: Iron & Blood";

  struct GroundUniforms {
    GLint view;
    GLint projection;
    GLint grass;
    GLint noise;
    GLint fogColor;
    GLint fogStart;
    GLint fogEnd;

    GroundUniforms(GLint v, GLint p, GLint g, GLint n, GLint fc, GLint fs, GLint fe)
      : view(v), projection(p), grass(g), noise(n), fogColor(fc), fogStart(fs), fogEnd(fe) {}
  };

  struct ObjUniforms {
    GLint view;
    GLint projection;
    GLint model;

    ObjUniforms(GLint v, GLint p, GLint m)
      : view(v), projection(p), model(m) {}
  };

  struct PathUniforms {
    GLint view;
    GLint projection;
    GLint model;
    GLint dirt;
    GLint noise;
    GLint fogColor;
    GLint fogStart;
    GLint fogEnd;

    PathUniforms(GLint v, GLint p, GLint m, GLint d, GLint n, GLint fc, GLint fs, GLint fe)
    : view(v), projection(p), model(m), dirt(d), noise(n), fogColor(fc), fogStart(fs), fogEnd(fe) {}
  };

  struct LineUniforms {
    GLint view;
    GLint projection;

    LineUniforms(GLint v, GLint p)
      : view(v), projection(p) {}
  };

  struct GpuMesh {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    GLsizei vertexCount = 0;
  };

  struct TreeInstance {
    Vector<3> position;
    float rotationY = 0.0f;
    float scale = 1.0f;
  };

  struct LanternInstance {
    Vector<3> position;
    float rotationY = 0.0f;
  };

  struct LanternUniforms {
    GLint view;
    GLint projection;
    GLint model;

    LanternUniforms(GLint v, GLint p, GLint m)
      : view(v), projection(p), model(m) {}
  };

  // Stats de inimigo (HP, velocidade no path, dano ao castelo).
  // Para novos tipos basta instanciar com valores diferentes — a logica de
  // update e generica.
  struct EnemyStats {
    int maxHp;
    float speed;
    int damage;
  };

  // Estado runtime de um inimigo unico percorrendo o path.
  struct EnemyInstance {
    EnemyStats stats;
    int hp;
    float pathDistance;
    bool alive;
    float respawnTimer;
    // Tempo restante de flash vermelho ao receber hit.
    float hitFlashTime;
  };

  // Estado de tiro de cada defensor (paralelo ao vector defenders).
  struct DefenderShoot {
    float shootTimer;
    bool aiming;
  };

  AppState &stateFromWindow(GLFWwindow *window) {
    return *static_cast<AppState *>(glfwGetWindowUserPointer(window));
  }

  unsigned int loadTexture(const char *path, int forcedChannels = 0) {
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Alinhamento 1 cobre texturas com largura "estranha" (alguns JPGs).
    // Restauramos para 4 ao final para não afetar uploads futuros.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    int w, h, channels;
    unsigned char *data = stbi_load(path, &w, &h, &channels, forcedChannels);


    if (!data) {
      std::cout << "ERRO: Falha ao carregar a textura " << path << std::endl;
      glDeleteTextures(1, &tex);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
      return 0;
    }

    const int actualChannels = (forcedChannels != 0) ? forcedChannels : channels;
    GLenum format;
    switch (actualChannels) {
        case 1:  format = GL_RED;  break;
        case 2:  format = GL_RG;   break;
        case 4:  format = GL_RGBA; break;
        default: format = GL_RGB;  break;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    return tex;
  }

  GpuMesh createSkyboxMesh() {
      GpuMesh m;
      float vertices[] = {
          // Posições X, Y, Z
          -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
          1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
          -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
          -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
          1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
          1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
          -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
          1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
          -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
          1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
          -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
          1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
      };

      glGenVertexArrays(1, &m.vao);
      glGenBuffers(1, &m.vbo);
      glBindVertexArray(m.vao);
      glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
      glEnableVertexAttribArray(0);
      m.vertexCount = 36;
      return m;
  }

  // Cria um plano [-200, 200] no XZ com UVs, usado como chão de grama.
  GpuMesh createGrassMesh() {
    GpuMesh m;
    // clang-format off
    static const float vertices[] = {
      // X,     Y,     Z,        U,      V
      // Primeiro triângulo
      -200.0f, 0.0f, -200.0f,    0.0f,   200.0f,   // Trás esquerda
       200.0f, 0.0f, -200.0f,    200.0f, 200.0f,   // Trás direita
      -200.0f, 0.0f,  200.0f,    0.0f,   0.0f,     // Frente esquerda
      // Segundo triângulo
       200.0f, 0.0f, -200.0f,    200.0f, 200.0f,   // Trás direita
       200.0f, 0.0f,  200.0f,    200.0f, 0.0f,     // Frente direita
      -200.0f, 0.0f,  200.0f,    0.0f,   0.0f,     // Frente esquerda
    };
    // clang-format on

    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    m.vertexCount = 6;
    return m;
  }

  // Cria a mesh da curva-guia (line strip). Apenas posição (3 floats).
  GpuMesh createCurveMesh(const std::vector<Point> &curvePoints) {
    GpuMesh m;
    std::vector<float> data;
    data.reserve(curvePoints.size() * 3);
    for (const Point &p : curvePoints) {
      data.push_back(p.x);
      data.push_back(0.1f);  // Acima do chão para evitar z-fighting com a grama
      data.push_back(p.y);
    }

    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(
        GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    m.vertexCount = static_cast<GLsizei>(curvePoints.size());
    return m;
  }

  float distancePointSegment2D(const Point& p, const Point& a, const Point& b) {
    const float vx = b.x - a.x;
    const float vy = b.y - a.y;
    const float wx = p.x - a.x;
    const float wy = p.y - a.y;

    const float c1 = vx * wx + vy * wy;
    if (c1 <= 0.0f) {
      return std::sqrt(wx * wx + wy * wy);
    }

    const float c2 = vx * vx + vy * vy;
    if (c2 <= c1) {
      const float dx = p.x - b.x;
      const float dy = p.y - b.y;
      return std::sqrt(dx * dx + dy * dy);
    }

    const float t = c1 / c2;
    const float projx = a.x + t * vx;
    const float projy = a.y + t * vy;
    const float dx = p.x - projx;
    const float dy = p.y - projy;
    return std::sqrt(dx * dx + dy * dy);
  }

  float distanceToPath(const std::vector<Point>& curvePoints, float x, float z) {
    if (curvePoints.size() < 2) {
      return std::numeric_limits<float>::max();
    }

    const Point p{x, z};
    float minDist = std::numeric_limits<float>::max();
    for (size_t i = 0; i + 1 < curvePoints.size(); ++i) {
      const float d = distancePointSegment2D(p, curvePoints[i], curvePoints[i + 1]);
      if (d < minDist) {
        minDist = d;
      }
    }

    return minDist;
  }

  // =========================================================================
  // MOVIMENTAÇÃO NO PATH
  // =========================================================================

  struct PathCache {
    std::vector<float> accumulatedDistances;
    float totalDistance = 0.0f;
  };

  // Pré-calcula a distância de cada ponto até o início da curva
  PathCache buildPathCache(const std::vector<Point>& points) {
    PathCache cache;
    if (points.empty()) {
      std::cerr << "Erro: nenhum ponto enviado para buildPathCache." << std::endl;
      std::exit(EXIT_FAILURE);
    }

    // Primeiro valor sempre é 0
    cache.accumulatedDistances.push_back(0.0f);
    float total = 0.0f;

    // Percorre os segmentos calculando o comprimento e acumulando distância
    for (size_t i = 0; i < points.size() - 1; ++i) {
      float dx = points[i + 1].x - points[i].x;
      float dy = points[i + 1].y - points[i].y;
      total += std::sqrt(dx * dx + dy * dy);
      cache.accumulatedDistances.push_back(total);
    }

    cache.totalDistance = total;
    return cache;
  }

  // =========================================================================
  // CÂMERA ORBITAL
  // =========================================================================

  void updateOrbitalCameraPosition(const AppState &s, Vector<3> &cameraPos) {
    cameraPos[0] = s.orbitRadius * std::cos(s.orbitPitch) * std::sin(s.orbitYaw);
    cameraPos[1] = s.orbitRadius * std::sin(s.orbitPitch);
    cameraPos[2] = s.orbitRadius * std::cos(s.orbitPitch) * std::cos(s.orbitYaw);
  }

  // =========================================================================
  // CALLBACKS
  // =========================================================================

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

    xoffset *= kMouseSensitivity;
    yoffset *= kMouseSensitivity;

    s.yaw += xoffset;
    s.pitch += yoffset;

    if (s.pitch > kMaxPitchRad) s.pitch = kMaxPitchRad;
    if (s.pitch < -kMaxPitchRad) s.pitch = -kMaxPitchRad;
  }

  // "/*xoffset*/" pois é um parâmetro não usado mas que a assinatura pede
  // Evita warning do compilador
  void scrollCallback(GLFWwindow *window, double /*xoffset*/, double yoffset) {
    AppState &s = stateFromWindow(window);
    if (s.useFreeCamera) {
      return;
    }
    s.orbitRadius -= static_cast<float>(yoffset);
    if (s.orbitRadius < kMinOrbitalRadius) s.orbitRadius = kMinOrbitalRadius;
    if (s.orbitRadius > kMaxOrbitalRadius) s.orbitRadius = kMaxOrbitalRadius;
  }

  void framebufferSizeCallback(GLFWwindow *window, int width, int height) {
    AppState &s = stateFromWindow(window);
    s.fbWidth = width;
    s.fbHeight = height;
    glViewport(0, 0, width, height);
  }

  // =========================================================================
  // INPUT
  // =========================================================================

  void processInput(GLFWwindow *window, Vector<3> &cameraPosition, float deltaTime) {
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

  // bool hasReachedEnd é usado para detectar o final da curva
  // Serve para saber quando o adversário chegou no objetivo
  Vector<3> getPositionAtDistance(
    const std::vector<Point> &points,
    const PathCache &cache,
    float distance,
    float &outAngle,
    bool &hasReachedEnd
  ) {
    // Caso inválido
    if (points.size() < 2 || cache.totalDistance <= 0.0f) {
      std::cerr << "Erro: path inválido." << std::endl;
      std::exit(EXIT_FAILURE);
    }

    if (distance <= 0.0f) {
      distance = 0.0f;
    } else if (distance >= cache.totalDistance) {
      distance = cache.totalDistance;
      hasReachedEnd = true;
    }

    // Encontra o segmento em que o personagem está
    auto it = std::upper_bound(cache.accumulatedDistances.begin(), cache.accumulatedDistances.end(), distance);
    size_t i = std::distance(cache.accumulatedDistances.begin(), it) - 1;

    // Proteção para não estourar o índice do vetor quando distance == totalDistance
    if (i >= points.size() - 1) {
      i = points.size() - 2;
    }

    // Encontra os pontos desse segmento
    // O personagem está entre eles
    Point p1 = points[i];
    Point p2 = points[i + 1];

    // Encontra a distância relativa
    float distToP1 = distance - cache.accumulatedDistances[i];
    float segLen = cache.accumulatedDistances[i + 1] - cache.accumulatedDistances[i];
    float t = distToP1 / segLen;

    // Atualiza com direção do movimento e ângulo
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    outAngle = std::atan2(-dx, dy);

    return {p1.x + dx * t, 0.1f, p1.y + dy * t};
  }

  // =========================================================================
  // EXECUÇÃO PRINCIPAL
  // =========================================================================
  // Tudo que aloca recurso GL vive aqui dentro. Quando run() retorna, todos
  // os recursos foram liberados e os destrutores de Mesh já rodaram, ainda
  // com o contexto GL válido. Só depois main() chama glfwDestroyWindow

  //F.aux RAYCASTING
  Vector<3> GetMouseGroundPos(GLFWwindow* window, const Camera& cam, const Vector<3>& camPos, int width, int height) {
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    // 1. Converte a posição do mouse para coordenadas normalizadas (NDC) [-1, 1]
    float x = (2.0f * static_cast<float>(mouseX)) / static_cast<float>(width) - 1.0f;
    float y = 1.0f - (2.0f * static_cast<float>(mouseY)) / static_cast<float>(height);

    // 2. Extrair os eixos (Right, Up, Forward) diretamente da Matriz de Visão (View Matrix)
    // Na sua implementação, a rotação da câmera fica transposta na matriz 3x3 do canto superior esquerdo.
    Matrix<4, 4> V = cam.getViewMatrix();
    Vector<3> right    { V(0,0), V(0,1), V(0,2) };
    Vector<3> up       { V(1,0), V(1,1), V(1,2) };
    Vector<3> backward { V(2,0), V(2,1), V(2,2) };

    // 3. Pegar os parâmetros de Projeção que você usou na câmera
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    // kFovDegrees vem do seu arquivo main.cpp (45.0f)
    float tanHalfFov = std::tan((kFovDegrees * math_constants::kDegToRad) / 2.0f);

    // 4. Calcular o vetor de direção do raio no espaço da câmera (Z = -1)
    float vx = x * aspect * tanHalfFov;
    float vy = y * tanHalfFov;
    float vz = -1.0f;

    // 5. Converter do View Space para o World Space multiplicando pelos vetores base
    Vector<3> rayWorld;
    rayWorld[0] = vx * right[0] + vy * up[0] + vz * backward[0];
    rayWorld[1] = vx * right[1] + vy * up[1] + vz * backward[1];
    rayWorld[2] = vx * right[2] + vy * up[2] + vz * backward[2];

    // Normalizar o raio (deixar tamanho = 1)
    float length = std::sqrt(rayWorld[0]*rayWorld[0] + rayWorld[1]*rayWorld[1] + rayWorld[2]*rayWorld[2]);
    rayWorld[0] /= length;
    rayWorld[1] /= length;
    rayWorld[2] /= length;

    // 6. Encontrar o ponto de interseção exato com o plano do chão (Y = 0)
    // Equação da reta: P = Origem + Raio * t -> Queremos P.y = 0
    if (rayWorld[1] >= 0.0f) {
        return Vector<3>{0.0f, 0.0f, 0.0f};
    }

    float t = -camPos[1] / rayWorld[1];

    Vector<3> result;
    result[0] = camPos[0] + rayWorld[0] * t;
    result[1] = 0.0f;
    result[2] = camPos[2] + rayWorld[2] * t;

    return result;
}

  int run(GLFWwindow *window) {
    AppState state;
    glfwSetWindowUserPointer(window, &state);

    // Sincroniza tamanho real do framebuffer
    glfwGetFramebufferSize(window, &state.fbWidth, &state.fbHeight);
    glViewport(0, 0, state.fbWidth, state.fbHeight);

    glEnable(GL_DEPTH_TEST);

    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    Hud gameHud;
    gameHud.Init(window);
    //passar pra um loader dedicado depois
    HudTextures uiTextures;
      uiTextures.topBackground = loadTexture("data/textures/ui_topbg.png", 4);
      uiTextures.goldIcon      = loadTexture("data/textures/ui_gold.png", 4);
      uiTextures.healthIcon    = loadTexture("data/textures/ui_health.png", 4);
      uiTextures.archerIcon    = loadTexture("data/textures/ui_archer.png", 4);

      gameHud.SetTextures(uiTextures);

    // ---------------------------------------------------------------------
    // SHADERS
    // ---------------------------------------------------------------------
    // groundShader: chão de grama
    // objShader:    entidades
    // pathShader:   caminho de terra
    // lineShader:   curva-guia amarela
    // skyShader:    céu estrelado
    unsigned int groundShader = createShaderProgram("data/shaders/grass.vert", "data/shaders/grass.frag");
    unsigned int objShader = createShaderProgram("data/shaders/shader.vert", "data/shaders/shader.frag");
    unsigned int pathShader = createShaderProgram("data/shaders/path.vert", "data/shaders/path.frag");
    unsigned int lineShader = createShaderProgram("data/shaders/line.vert", "data/shaders/line.frag");
    unsigned int skyShader = createShaderProgram("data/shaders/sky.vert", "data/shaders/sky.frag");
    unsigned int lanternShader = createShaderProgram("data/shaders/lantern.vert", "data/shaders/lantern.frag");
    unsigned int previewShader = createShaderProgram("data/shaders/preview.vert", "data/shaders/preview.frag");


    if (!groundShader || !objShader || !pathShader || !lineShader || !lanternShader) {
      std::cout << "ERRO: Falha ao criar um ou mais shaders" << std::endl;
      glDeleteProgram(groundShader);
      glDeleteProgram(objShader);
      glDeleteProgram(pathShader);
      glDeleteProgram(lineShader);
      glDeleteProgram(lanternShader);
      glDeleteProgram(previewShader);
      return 1;
    }

    // Cache das uniform locations
    GroundUniforms groundU{
        glGetUniformLocation(groundShader, "view"),
        glGetUniformLocation(groundShader, "projection"),
        glGetUniformLocation(groundShader, "grass"),
        glGetUniformLocation(groundShader, "noise"),
        glGetUniformLocation(groundShader, "fogColor"),
        glGetUniformLocation(groundShader, "fogStart"),
        glGetUniformLocation(groundShader, "fogEnd"),
    };
    ObjUniforms objU{
        glGetUniformLocation(objShader, "view"),
        glGetUniformLocation(objShader, "projection"),
        glGetUniformLocation(objShader, "model"),
    };
    PathUniforms pathU{
        glGetUniformLocation(pathShader, "view"),
        glGetUniformLocation(pathShader, "projection"),
        glGetUniformLocation(pathShader, "model"),
        glGetUniformLocation(pathShader, "dirt"),
      glGetUniformLocation(pathShader, "noise"),
      glGetUniformLocation(pathShader, "fogColor"),
      glGetUniformLocation(pathShader, "fogStart"),
      glGetUniformLocation(pathShader, "fogEnd"),
    };
    LineUniforms lineU{
        glGetUniformLocation(lineShader, "view"),
        glGetUniformLocation(lineShader, "projection"),
    };
    LanternUniforms lanternU{
        glGetUniformLocation(lanternShader, "view"),
        glGetUniformLocation(lanternShader, "projection"),
        glGetUniformLocation(lanternShader, "model"),
    };

    const glm::vec3 fogColor(0.02f, 0.03f, 0.07f);
    const float fogStart = 60.0f;
    const float fogEnd = 160.0f;

    // Texture units são constantes — setamos uma vez só (era refeito todo frame).
    glUseProgram(groundShader);
    glUniform1i(groundU.grass, 0);
    glUniform1i(groundU.noise, 1);
    glUniform1i(glGetUniformLocation(groundShader, "normalMap"),       2);
    glUniform1i(glGetUniformLocation(groundShader, "aoMap"),           3);
    glUniform1i(glGetUniformLocation(groundShader, "roughnessMap"),    4);
    glUniform1i(glGetUniformLocation(groundShader, "displacementMap"), 5);
    glUniform3fv(groundU.fogColor, 1, glm::value_ptr(fogColor));
    glUniform1f(groundU.fogStart, fogStart);
    glUniform1f(groundU.fogEnd, fogEnd);

    glUseProgram(pathShader);
    glUniform1i(pathU.dirt, 0);
    glUniform1i(pathU.noise, 1);
    glUniform1i(glGetUniformLocation(pathShader, "normalMap"),       2);
    glUniform1i(glGetUniformLocation(pathShader, "aoMap"),           3);
    glUniform1i(glGetUniformLocation(pathShader, "roughnessMap"),    4);
    glUniform1i(glGetUniformLocation(pathShader, "displacementMap"), 5);
    glUniform3fv(pathU.fogColor, 1, glm::value_ptr(fogColor));
    glUniform1f(pathU.fogStart, fogStart);
    glUniform1f(pathU.fogEnd, fogEnd);

    glUseProgram(objShader);
    glUniform3fv(glGetUniformLocation(objShader, "fogColor"), 1, glm::value_ptr(fogColor));
    glUniform1f(glGetUniformLocation(objShader, "fogStart"), fogStart);
    glUniform1f(glGetUniformLocation(objShader, "fogEnd"), fogEnd);

    glUseProgram(lanternShader);
    glUniform1i(glGetUniformLocation(lanternShader, "colorMap"),     0);
    glUniform1i(glGetUniformLocation(lanternShader, "normalMap"),    1);
    glUniform1i(glGetUniformLocation(lanternShader, "roughnessMap"), 2);
    glUniform1i(glGetUniformLocation(lanternShader, "metallicMap"),  3);
    glUniform1i(glGetUniformLocation(lanternShader, "aoMap"),        4);
    glUniform1i(glGetUniformLocation(lanternShader, "opacityMap"),   5);
    glUniform3fv(glGetUniformLocation(lanternShader, "fogColor"), 1, glm::value_ptr(fogColor));
    glUniform1f(glGetUniformLocation(lanternShader, "fogStart"), fogStart);
    glUniform1f(glGetUniformLocation(lanternShader, "fogEnd"), fogEnd);

    // ---------------------------------------------------------------------
    // CARREGA OBJ/GLB ANTES DE ALOCAR MAIS RECURSOS GL
    // ---------------------------------------------------------------------
    std::vector<Vertex> objVertices;
    AnimatedModel enemyBase("data/models/zombie/zombieT.glb");
    enemyBase.LoadAnimation("run", "data/models/zombie/zombieRun.glb");
    GameObject enemyRunner(&enemyBase, Vector<3>{0.0f, 0.0f, 0.0f});
    enemyRunner.SetIdleAnimations({"run"});
    unsigned int enemyTexture = loadTexture("data/textures/zombie.png");

    AnimatedModel arquebusBase("data/models/arquebus/arquebusT.glb");
    arquebusBase.LoadAnimation("idle1", "data/models/arquebus/arquebusIdle.glb");
    unsigned int arquebusTex = loadTexture("data/textures/arquebus.png");
    unsigned int arquebusNormalTex = loadTexture("data/textures/arquebusnormal.png");

    std::vector<Vertex> bowVertices;
    if (!Parser("data/models/Archer/bow.obj", bowVertices)) {
      std::cout << "ERRO: Nao encontrou data/models/Archer/bow.obj" << std::endl;
    }

    std::vector<Vertex> arquebusVertices;
    if (!Parser("data/models/arquebus/arquebusweapon.obj", arquebusVertices)) {
      std::cout << "ERRO: Nao encontrou arquebusweapon.obj" << std::endl;
    }
    Mesh arquebusMesh(arquebusVertices);
    unsigned int arquebusTexture = loadTexture("data/textures/arquebusweapon.png");

    std::vector<Vertex> castleVertices;
    Parser("data/models/world/castle.obj", castleVertices);
    Mesh castleMesh(castleVertices);
    unsigned int castleTex = loadTexture("data/textures/castle.png");
    unsigned int castleNormalTex = loadTexture("data/textures/castlenormal.png");

    std::vector<Vertex> treeLeavesVertices;
    std::unique_ptr<Mesh> treeLeavesMesh;
    if (!Parser("data/models/world/leaves.obj", treeLeavesVertices)) {
      std::cout << "ERRO: Nao encontrou data/models/world/leaves.obj" << std::endl;
    } else if (!treeLeavesVertices.empty()) {
      treeLeavesMesh.reset(new Mesh(treeLeavesVertices));
    } else {
      std::cout << "[trees] leaves.obj sem vertices (formato nao suportado?)" << std::endl;
    }

    std::vector<Vertex> treeLogVertices;
    std::unique_ptr<Mesh> treeLogMesh;
    if (!Parser("data/models/world/log.obj", treeLogVertices)) {
      std::cout << "ERRO: Nao encontrou data/models/world/log.obj" << std::endl;
    } else if (!treeLogVertices.empty()) {
      treeLogMesh.reset(new Mesh(treeLogVertices));
    } else {
      std::cout << "[trees] log.obj sem vertices (formato nao suportado?)" << std::endl;
    }

    bool usingFallbackTree = false;
    if (!treeLeavesMesh && !treeLogMesh) {
      if (Parser("data/models/world/tree.obj", treeLogVertices)) {
        if (!treeLogVertices.empty()) {
          treeLogMesh.reset(new Mesh(treeLogVertices));
          usingFallbackTree = true;
          std::cout << "[trees] Fallback: carregou data/models/world/tree.obj" << std::endl;
        } else {
          std::cout << "[trees] Fallback sem vertices (formato nao suportado?)" << std::endl;
        }
      } else {
        std::cout << "[trees] Fallback falhou: data/models/world/tree.obj" << std::endl;
      }
    }

    // ---------------------------------------------------------------------
    // GEOMETRIA
    // ---------------------------------------------------------------------
    GpuMesh grass = createGrassMesh();

    // Atualmente simula f(x) = x * cos(x), x ∈ [-2, 2] -> [-10, 10] em 20 pontos
    std::vector<Point> controlPoints = {
        {-10.0000f, -4.1615f}, {-8.9474f, -1.9551f}, {-7.8947f, -0.0643f},
        {-6.8421f, 1.3658f},   {-5.7895f, 2.3234f},  {-4.7368f, 2.7654f},
        {-3.6842f, 2.7286f},   {-2.6316f, 2.2750f},  {-1.5789f, 1.5009f},
        {-0.5263f, 0.5234f},   {0.5263f, -0.5234f},  {1.5789f, -1.5009f},
        {2.6316f, -2.2750f},   {3.6842f, -2.7286f},  {4.7368f, -2.7654f},
        {5.7895f, -2.3234f},   {6.8421f, -1.3658f},  {7.8947f, 0.0643f},
        {8.9474f, 1.9551f},    {10.0000f, 4.1615f}};
    std::vector<Point> curvePoints = generateCatmullRomVertices(controlPoints);
    GpuMesh curve = createCurveMesh(curvePoints);
    PathCache curveCache = buildPathCache(curvePoints);

    const float kMapHalfSize = 200.0f;
    const float kTreeBorderMargin = 12.0f;
    const float kTreeMinSpacing = 6.0f;
    const float kTreePathBuffer = 6.0f;
    const float kTreeBaseScale = 1.0f;
    const int kTreeCount = 1000;
    const int kMaxTreeAttempts = kTreeCount * 40;
    const float kPathHalfWidth = 2.0f;

    std::vector<TreeInstance> trees;
    trees.reserve(kTreeCount);

    std::mt19937 rng(static_cast<unsigned int>(time(NULL)));
    std::uniform_real_distribution<float> posDist(
        -kMapHalfSize + kTreeBorderMargin,
        kMapHalfSize - kTreeBorderMargin);
    std::uniform_real_distribution<float> rotDist(0.0f, math_constants::kTwoPi);
    std::uniform_real_distribution<float> scaleDist(0.8f, 1.4f);

    const float minSpacingSq = kTreeMinSpacing * kTreeMinSpacing;
    const float pathAvoidDist = kPathHalfWidth + kTreePathBuffer;
    int attempts = 0;
    while (static_cast<int>(trees.size()) < kTreeCount && attempts < kMaxTreeAttempts) {
      ++attempts;

      const float x = posDist(rng);
      const float z = posDist(rng);

      if (distanceToPath(curvePoints, x, z) < pathAvoidDist) {
        continue;
      }

      bool tooClose = false;
      for (const auto& t : trees) {
        const float dx = x - t.position[0];
        const float dz = z - t.position[2];
        if ((dx * dx + dz * dz) < minSpacingSq) {
          tooClose = true;
          break;
        }
      }
      if (tooClose) {
        continue;
      }

      TreeInstance tree;
      tree.position = Vector<3>{x, 0.0f, z};
      tree.rotationY = rotDist(rng);
      tree.scale = kTreeBaseScale * scaleDist(rng);
      trees.push_back(tree);
    }

    Mesh pathMesh = generatePathMesh(curvePoints, 2.0f);

    // ---------------------------------------------------------------------
    // LANTERNAS — 20 unidades alternadas (zigue-zague) ao longo do path
    // ---------------------------------------------------------------------
    std::vector<Vertex> lanternVertices;
    std::unique_ptr<Mesh> lanternMesh;
    if (!Parser("data/models/world/lantern.obj", lanternVertices)) {
      std::cout << "ERRO: Nao encontrou data/models/world/lantern.obj" << std::endl;
    } else if (!lanternVertices.empty()) {
      lanternMesh.reset(new Mesh(lanternVertices));
    } else {
      std::cout << "[lanterns] lantern.obj sem vertices (formato nao suportado?)" << std::endl;
    }

    const int kLanternCount = 8;
    const float kLanternScale = 0.01f;
    // Distância do centro do path; path tem meia-largura 2.0, então fica na grama.
    const float kLanternPathOffset = 2.8f;
    // Altura aproximada do bulbo/chama, em world units (já escalado).
    // BBox do OBJ: Y ∈ [-5, 85]cm; bulbo no meio ~ y=45cm → 0.9 world units.
    const float kLanternLightHeight = 0.9f;
    // Laranja quente; valores > 1 para a luz "estourar" sobre o ambient noturno.
    const glm::vec3 kLanternLightColor = glm::vec3(1.8f, 1.05f, 0.40f);

    std::vector<LanternInstance> lanterns;
    lanterns.reserve(kLanternCount);
    std::vector<PointLight> lanternLights;
    lanternLights.reserve(kLanternCount);

    if (curveCache.totalDistance > 0.0f) {
      // Duas sequências interleaved: 10 lanternas em cada lado, com espaçamento
      // próprio uniforme. Lado direito sai 0.5 spacing à frente do esquerdo para
      // o efeito zigue-zague. Evita aglomeração em curvas.
      const int perSide = kLanternCount / 2;
      const float spacing = curveCache.totalDistance / static_cast<float>(perSide);
      for (int i = 0; i < kLanternCount; ++i) {
        const int sideIdx = i / 2;
        const bool isLeft = (i % 2 == 0);
        const float phase = isLeft ? 0.25f : 0.75f;
        const float arcLen = (static_cast<float>(sideIdx) + phase) * spacing;

        float pathAngle = 0.0f;
        bool dummyReachedEnd = false;
        Vector<3> pathPos = getPositionAtDistance(
            curvePoints, curveCache, arcLen, pathAngle, dummyReachedEnd);

        // getPositionAtDistance retorna outAngle = atan2(-dx, dy). Recupera a
        // direção 2D normalizada do path neste ponto.
        const float ndx = -std::sin(pathAngle);
        const float ndy =  std::cos(pathAngle);

        // Perpendicular esquerda em XZ (rot 90° anti-horário em torno de Y).
        const float perpX = -ndy;
        const float perpZ =  ndx;
        const float side = isLeft ? 1.0f : -1.0f;

        LanternInstance lantern;
        lantern.position = Vector<3>{
            pathPos[0] + perpX * kLanternPathOffset * side,
            0.0f,
            pathPos[2] + perpZ * kLanternPathOffset * side};

        // Lanterna olha para o centro do path: vetor da lanterna até o path.
        const float towardPathX = -perpX * side;
        const float towardPathZ = -perpZ * side;
        lantern.rotationY = std::atan2(towardPathX, towardPathZ);

        lanterns.push_back(lantern);

        PointLight pl;
        pl.position = glm::vec3(
            lantern.position[0], kLanternLightHeight, lantern.position[2]);
        pl.color = kLanternLightColor;
        lanternLights.push_back(pl);
      }
    }

    unsigned int lanternColorTex      = loadTexture("data/textures/lantern_color.jpg");
    unsigned int lanternNormalTex     = loadTexture("data/textures/lantern_normal.jpg");
    unsigned int lanternRoughnessTex  = loadTexture("data/textures/lantern_roughness.jpg");
    unsigned int lanternMetallicTex   = loadTexture("data/textures/lantern_metallic.jpg");
    unsigned int lanternAOTex         = loadTexture("data/textures/lantern_mixed_ambient_occlusion.jpg");
    unsigned int lanternOpacityTex    = loadTexture("data/textures/lantern_opacity.jpg");

    Mesh bowMesh(bowVertices);
    const bool hasTreeLeavesMesh = (treeLeavesMesh != nullptr);
    const bool hasTreeLogMesh = (treeLogMesh != nullptr);
    if (!hasTreeLeavesMesh && !hasTreeLogMesh) {
      std::cout << "[trees] nenhum mesh de arvore carregado; nao vai desenhar." << std::endl;
    }

    // ---------------------------------------------------------------------
    // TEXTURAS
    // ---------------------------------------------------------------------
    unsigned int grassTexture         = loadTexture("data/textures/grass_color.png");
    unsigned int grassNormalTex       = loadTexture("data/textures/grass_normal.png");
    unsigned int grassAOTex           = loadTexture("data/textures/grass_ambient_occlusion.png");
    unsigned int grassRoughnessTex    = loadTexture("data/textures/grass_roughness.png");
    unsigned int grassDisplacementTex = loadTexture("data/textures/grass_displacement.png");
    unsigned int dirtTexture          = loadTexture("data/textures/dirt_color.png");
    unsigned int dirtNormalTex        = loadTexture("data/textures/dirt_normal.png");
    unsigned int dirtAOTex            = loadTexture("data/textures/dirt_ambient_occlusion.png");
    unsigned int dirtRoughnessTex     = loadTexture("data/textures/dirt_roughness.png");
    unsigned int dirtDisplacementTex  = loadTexture("data/textures/dirt_displacement.png");
    unsigned int noiseTexture         = loadTexture("data/textures/perlin_noise.jpg", 3);
    unsigned int archerTexture        = loadTexture("data/textures/archer.png");
    unsigned int archerNormalTex      = loadTexture("data/textures/archernormal.png");
    unsigned int bowTexture           = loadTexture("data/textures/bow.jpg");
    unsigned int treeLeavesTexture    = loadTexture("data/textures/leaves.png", 4);
    unsigned int treeLogTexture       = loadTexture("data/textures/log.jpeg");
    unsigned int skyTexture           = loadTexture("data/textures/night_sky_tonemapped.jpg");

    GpuMesh skyMesh = createSkyboxMesh();
    GLint skyViewLoc = glGetUniformLocation(skyShader, "view");
    GLint skyProjLoc = glGetUniformLocation(skyShader, "projection");
    GLint skyTexLoc  = glGetUniformLocation(skyShader, "skyTexture");

    // ---------------------------------------------------------------------
    // INIMIGOS
    // ---------------------------------------------------------------------
    // Stats por tipo — quando adicionar novos inimigos, declare aqui.
    const EnemyStats kZombieStats{50, 2.0f, 20};

    EnemyInstance enemy;
    enemy.stats = kZombieStats;
    enemy.hp = enemy.stats.maxHp;
    enemy.pathDistance = 0.0f;
    enemy.alive = true;
    enemy.respawnTimer = 0.0f;
    enemy.hitFlashTime = 0.0f;

    const float kEnemyRespawnDelay = 2.0f;
    const float kArcherRange = 10.0f;
    const float kArcherShootInterval = 1.0f;
    const int kArcherArrowDamage = 10;
    const float kEnemyHitFlashDuration = 0.15f;

    // ---------------------------------------------------------------------
    // LOOP PRINCIPAL
    // ---------------------------------------------------------------------
    Camera cam;
    Vector<3> cameraPosition{0.0f, 2.0f, 5.0f};
    Matrix<4, 4> identity = Matrix<4, 4>::identity();

    unsigned int shaderAnim = createShaderProgram("data/shaders/anim_shader.vert", "data/shaders/anim_shader.frag");
    AnimatedModel archerBase("data/models/Archer/ArcherT.glb");

    archerBase.LoadAnimation("idle1", "data/models/Archer/Idle1.glb");
    archerBase.LoadAnimation("idle2", "data/models/Archer/Idle2.glb");
    archerBase.LoadAnimation("idle3", "data/models/Archer/Idle3.glb");
    archerBase.LoadAnimation("idle4", "data/models/Archer/Idle4.glb");
    archerBase.LoadAnimation("aim", "data/models/Archer/AimDraw.glb");
    std::vector<GameObject> defenders;
    std::vector<DefenderShoot> defenderShoots;

    // ---------------------------------------------------------------------
    // LUZ DIRECIONAL — lua noturna (fria, azulada)
    // Iluminação principal será das lanternas quando adicionadas.
    // ---------------------------------------------------------------------
    DirectionalLight moonLight;
    moonLight.direction = glm::normalize(glm::vec3(-0.4f, 1.2f, -0.5f));
    // Sombras
    moonLight.ambient   = glm::vec3(0.07f, 0.08f, 0.14f);
    // Difuso
    moonLight.diffuse   = glm::vec3(0.56f, 0.58f, 0.82f);
    // Especular
    moonLight.specular  = glm::vec3(0.14f, 0.16f, 0.26f);

    // ---- VARIÁVEIS DE CONTROLE DE TEMPO ----
    const double targetFPS = 60.0;
    const double frameDelay = 1.0 / targetFPS;
    double lastTime = glfwGetTime();
    unsigned char flatNormal[3] = {128, 128, 255};
    //normal fallback
    unsigned int defaultNormal;
    glGenTextures(1, &defaultNormal);
    glBindTexture(GL_TEXTURE_2D, defaultNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, flatNormal);
    while (!glfwWindowShouldClose(window)) {
      double currentTime = glfwGetTime();

      if (currentTime - lastTime < frameDelay) {
          continue;
      }

      const float deltaTime = static_cast<float>(frameDelay);

      // -------------------------------------------------------------------
      // UPDATE INIMIGO
      // -------------------------------------------------------------------
      if (enemy.alive) {
        enemy.pathDistance += enemy.stats.speed * deltaTime;
        enemyRunner.Update(deltaTime);
      } else {
        enemy.respawnTimer -= deltaTime;
        if (enemy.respawnTimer <= 0.0f) {
          enemy.hp = enemy.stats.maxHp;
          enemy.pathDistance = 0.0f;
          enemy.alive = true;
          enemy.hitFlashTime = 0.0f;
        }
      }

      if (enemy.hitFlashTime > 0.0f) {
        enemy.hitFlashTime -= deltaTime;
        if (enemy.hitFlashTime < 0.0f) enemy.hitFlashTime = 0.0f;
      }

      // Posicao atual do inimigo no path (somente faz sentido quando vivo).
      float characterAngle = 0.0f;
      bool hasReachedEnd = false;
      Vector<3> characterPos = getPositionAtDistance(
          curvePoints, curveCache, enemy.pathDistance, characterAngle, hasReachedEnd);

      if (enemy.alive && hasReachedEnd) {
        state.health -= enemy.stats.damage;
        if (state.health < 0) state.health = 0;
        enemy.alive = false;
        enemy.respawnTimer = kEnemyRespawnDelay;
      }

      // -------------------------------------------------------------------
      // UPDATE DEFENSORES (anima + logica de tiro)
      // -------------------------------------------------------------------
      if (defenderShoots.size() < defenders.size()) {
        defenderShoots.resize(defenders.size(), DefenderShoot{0.0f, false});
      }

      for (size_t i = 0; i < defenders.size(); ++i) {
        auto &unit = defenders[i];
        auto &shoot = defenderShoots[i];

        // Apenas arqueiros (type 1) atiram por enquanto — arquebus nao tem
        // animacao de aim carregada.
        if (unit.type == 1) {
          bool canShoot = false;
          float dxToEnemy = 0.0f;
          float dzToEnemy = 0.0f;
          if (enemy.alive) {
            dxToEnemy = characterPos[0] - unit.position[0];
            dzToEnemy = characterPos[2] - unit.position[2];
            const float distSq = dxToEnemy * dxToEnemy + dzToEnemy * dzToEnemy;
            canShoot = (distSq <= kArcherRange * kArcherRange);
          }

          if (canShoot) {
            // Vira o arqueiro para o inimigo (mesma convencao usada pelas lanternas
            // e pelo enemyRunner que segue o path).
            unit.rotationY = -std::atan2(dxToEnemy, dzToEnemy);

            if (!shoot.aiming) {
              unit.SetAnimation("aim");
              shoot.aiming = true;
              shoot.shootTimer = 0.0f;
            }
            shoot.shootTimer += deltaTime;
            if (shoot.shootTimer >= kArcherShootInterval) {
              shoot.shootTimer -= kArcherShootInterval;
              enemy.hp -= kArcherArrowDamage;
              enemy.hitFlashTime = kEnemyHitFlashDuration;
              if (enemy.hp <= 0) {
                enemy.hp = 0;
                enemy.alive = false;
                enemy.respawnTimer = kEnemyRespawnDelay;
              }
            }
          } else if (shoot.aiming) {
            shoot.aiming = false;
            shoot.shootTimer = 0.0f;
            unit.SetIdleAnimations({"idle1"});
          }
        }

        unit.Update(deltaTime);
      }

      lastTime += frameDelay;

      processInput(window, cameraPosition, deltaTime);

      // Céu noturno — combina com o ambient azulado da lua
      // SOBREPOSTO PELA TEXTURA DO CÉU
      glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      // Aspect ratio respeita o tamanho atual da janela (resize callback)
      const float aspect =
          static_cast<float>(state.fbWidth) / static_cast<float>(state.fbHeight);
      cam.setPerspective(
          kFovDegrees * math_constants::kDegToRad, aspect, kNearPlane, kFarPlane);

      if (state.useFreeCamera) {
        cam.setFPS(cameraPosition, state.yaw, state.pitch);
      } else {
        updateOrbitalCameraPosition(state, cameraPosition);
        Vector<3> lookTarget{0.0f, 0.0f, 0.0f};
        cam.setLookAt(cameraPosition, lookTarget);
      }

      auto glView = toOpenGLMatrix(cam.getViewMatrix());
      auto glProj = toOpenGLMatrix(cam.getProjectionMatrix());

      const glm::vec3 glmViewPos(cameraPosition[0], cameraPosition[1], cameraPosition[2]);

      // -------------------------------------------------------------------
      // ENTIDADES OPACAS
      // -------------------------------------------------------------------
      // Mudança obj estático pra glb com animação
      glUseProgram(shaderAnim);
      applyDirectionalLight(shaderAnim, moonLight, glmViewPos);
      applyPointLights(shaderAnim, lanternLights);
      glUniformMatrix4fv(glGetUniformLocation(shaderAnim, "view"), 1, GL_FALSE, glView.data());
      glUniformMatrix4fv(glGetUniformLocation(shaderAnim, "projection"), 1, GL_FALSE, glProj.data());
      // Por padrao sem flash; cada Draw que quiser tinge sobrescreve.
      glUniform1f(glGetUniformLocation(shaderAnim, "hitFlash"), 0.0f);

      if (enemy.alive) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, enemyTexture);
        glUniform1i(glGetUniformLocation(shaderAnim, "tex"), 0);
        enemyRunner.position = characterPos;
        enemyRunner.rotationY = characterAngle;
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, defaultNormal);
        glUniform1i(glGetUniformLocation(shaderAnim, "normalMap"), 1);
        const float flash =
            (kEnemyHitFlashDuration > 0.0f)
                ? (enemy.hitFlashTime / kEnemyHitFlashDuration)
                : 0.0f;
        glUniform1f(glGetUniformLocation(shaderAnim, "hitFlash"), flash);
        enemyRunner.Draw(shaderAnim);
        // Reseta para nao vazar nos proximos desenhos (defensores, etc).
        glUniform1f(glGetUniformLocation(shaderAnim, "hitFlash"), 0.0f);
      }

      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, archerNormalTex);
      glUniform1i(glGetUniformLocation(shaderAnim, "normalMap"), 1);

      glUseProgram(shaderAnim);
      applyDirectionalLight(shaderAnim, moonLight, glmViewPos);
      applyPointLights(shaderAnim, lanternLights);
      glUniformMatrix4fv(glGetUniformLocation(shaderAnim, "view"), 1, GL_FALSE, glView.data());
      glUniformMatrix4fv(glGetUniformLocation(shaderAnim, "projection"), 1, GL_FALSE, glProj.data());

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, archerTexture);
      glUniform1i(glGetUniformLocation(shaderAnim, "tex"), 0);

      for (auto& unit : defenders) {

          if (unit.type == 1) {
              glActiveTexture(GL_TEXTURE0);
              glBindTexture(GL_TEXTURE_2D, archerTexture);
              glUniform1i(glGetUniformLocation(shaderAnim, "tex"), 0);

              glActiveTexture(GL_TEXTURE1);
              glBindTexture(GL_TEXTURE_2D, archerNormalTex);
              glUniform1i(glGetUniformLocation(shaderAnim, "normalMap"), 1);
          }
          else if (unit.type == 2) {
              glActiveTexture(GL_TEXTURE0);
              glBindTexture(GL_TEXTURE_2D, arquebusTex);
              glUniform1i(glGetUniformLocation(shaderAnim, "tex"), 0);

              glActiveTexture(GL_TEXTURE1);
              glBindTexture(GL_TEXTURE_2D, arquebusNormalTex);
              glUniform1i(glGetUniformLocation(shaderAnim, "normalMap"), 1);
          }

          // 2. Desenha a unidade
          unit.Draw(shaderAnim);
      }

      glUseProgram(objShader);
      applyDirectionalLight(objShader, moonLight, glmViewPos);
      applyPointLights(objShader, lanternLights);
      glUniformMatrix4fv(objU.view, 1, GL_FALSE, glView.data());
      glUniformMatrix4fv(objU.projection, 1, GL_FALSE, glProj.data());

      for (auto& unit : defenders) {
          if (unit.type == 1) {
              glm::mat4 handWorldMatrix = unit.GetBoneWorldTransform("mixamorig:LeftHand");

              glm::mat4 offset = glm::mat4(0.0f);
              float s = 5.0f;


              offset[0][2] = s;
              offset[1][1] = s;
              offset[2][0] = -s;
              offset[3][3] = 1.0f;

              offset[3][0] = 0.0f; offset[3][1] = -7.0f; offset[3][2] = 0.0f;
              glm::mat4 finalBowMatrix = handWorldMatrix * offset;

              glUniformMatrix4fv(objU.model, 1, GL_FALSE, glm::value_ptr(finalBowMatrix));

              glActiveTexture(GL_TEXTURE0);
              glBindTexture(GL_TEXTURE_2D, bowTexture);
              glUniform1i(glGetUniformLocation(objShader, "tex"), 0);

              bowMesh.Draw();
          }
          else if (unit.type == 2) {
              glm::mat4 handWorldMatrix = unit.GetBoneWorldTransform("mixamorig:LeftHand");

              float s = 8.0f;
              Matrix<4, 4> mScale = scale<4, 4>(s, s, s);
              Matrix<4, 4> mRotY = rotateY<4, 4>(-math_constants::kHalfPi);
              Matrix<4, 4> mRotX = rotateX<4, 4>(-20.0f * math_constants::kDegToRad);
              Matrix<4, 4> mRotZ = rotateZ<4, 4>(-math_constants::kHalfPi);
              Matrix<4, 4> mTrans = translate<4, 4>(0.0f, 0.0f, -4.0f);
              Matrix<4, 4> customOffset = mTrans * mRotY * mRotX * mScale *mRotZ;

              auto glCustomOffset = toOpenGLMatrix(customOffset);
              glm::mat4 glmOffset = glm::make_mat4(glCustomOffset.data());

              glm::mat4 finalWeaponMatrix = handWorldMatrix * glmOffset;

              glUniformMatrix4fv(objU.model, 1, GL_FALSE, glm::value_ptr(finalWeaponMatrix));
              glActiveTexture(GL_TEXTURE0);
              glBindTexture(GL_TEXTURE_2D, arquebusTexture);
              glUniform1i(glGetUniformLocation(objShader, "tex"), 0);

              arquebusMesh.Draw();
          }
         }
        if (!curvePoints.empty()) {
            // Pega o último ponto da curva gerada pelo Catmull-Rom
            Point endPoint = curvePoints.back();
            float offsetX = 4.0f;
            float offsetY = 0.0f;
            float offsetZ = 4.0;

            Matrix<4, 4> castleTranslate = translate<4, 4>(endPoint.x + offsetX, 0.0f + offsetY, endPoint.y + offsetZ);
            float anguloGiroGraus = -55.0f;
            Matrix<4, 4> castleRotY = rotateY<4, 4>(anguloGiroGraus * math_constants::kDegToRad);
            Matrix<4, 4> castleScale     = scale<4, 4>(0.072, 0.072f, 0.072f); // Ajuste o tamanho se necessário
            Matrix<4, 4> castleModel = castleTranslate * castleRotY * castleScale;
            auto glCastleModel = toOpenGLMatrix(castleModel);


            glUniformMatrix4fv(objU.model, 1, GL_FALSE, glCastleModel.data());
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, castleTex);
            glUniform1i(glGetUniformLocation(objShader, "tex"), 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, castleNormalTex);
            glUniform1i(glGetUniformLocation(objShader, "normalMap"), 1);

            castleMesh.Draw();

      }

      // -------------------------------------------------------------------
      // CHÃO DE GRAMA
      // -------------------------------------------------------------------
      glUseProgram(groundShader);
      applyDirectionalLight(groundShader, moonLight, glmViewPos);
      applyPointLights(groundShader, lanternLights);
      glUniformMatrix4fv(groundU.view, 1, GL_FALSE, glView.data());
      glUniformMatrix4fv(groundU.projection, 1, GL_FALSE, glProj.data());

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, grassTexture);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, noiseTexture);
      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, grassNormalTex);
      glActiveTexture(GL_TEXTURE3);
      glBindTexture(GL_TEXTURE_2D, grassAOTex);
      glActiveTexture(GL_TEXTURE4);
      glBindTexture(GL_TEXTURE_2D, grassRoughnessTex);
      glActiveTexture(GL_TEXTURE5);
      glBindTexture(GL_TEXTURE_2D, grassDisplacementTex);

      glBindVertexArray(grass.vao);
      glDrawArrays(GL_TRIANGLES, 0, grass.vertexCount);

      // -------------------------------------------------------------------
      // ARVORES
      // -------------------------------------------------------------------
      if ((hasTreeLeavesMesh || hasTreeLogMesh) && !trees.empty()) {
        glUseProgram(objShader);
        applyDirectionalLight(objShader, moonLight, glmViewPos);
        applyPointLights(objShader, lanternLights);
        glUniformMatrix4fv(objU.view, 1, GL_FALSE, glView.data());
        glUniformMatrix4fv(objU.projection, 1, GL_FALSE, glProj.data());

        if (hasTreeLogMesh && treeLogTexture != 0) {
          glActiveTexture(GL_TEXTURE0);
          glBindTexture(GL_TEXTURE_2D, treeLogTexture);
          glUniform1i(glGetUniformLocation(objShader, "tex"), 0);

          for (const auto& tree : trees) {
            Matrix<4, 4> treeTranslate = translate<4, 4>(
                tree.position[0], tree.position[1], tree.position[2]);
            Matrix<4, 4> treeRotateY = rotateY<4, 4>(tree.rotationY);
            Matrix<4, 4> treeScale = scale<4, 4>(tree.scale, tree.scale, tree.scale);
            Matrix<4, 4> treeModel = treeTranslate * treeRotateY * treeScale;
            auto glTreeModel = toOpenGLMatrix(treeModel);
            glUniformMatrix4fv(objU.model, 1, GL_FALSE, glTreeModel.data());
            treeLogMesh->Draw();
          }
        }

        if (hasTreeLeavesMesh && treeLeavesTexture != 0) {
          glEnable(GL_BLEND);
          glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

          glActiveTexture(GL_TEXTURE0);
          glBindTexture(GL_TEXTURE_2D, treeLeavesTexture);
          glUniform1i(glGetUniformLocation(objShader, "tex"), 0);

          for (const auto& tree : trees) {
            Matrix<4, 4> treeTranslate = translate<4, 4>(
                tree.position[0], tree.position[1], tree.position[2]);
            Matrix<4, 4> treeRotateY = rotateY<4, 4>(tree.rotationY);
            Matrix<4, 4> treeScale = scale<4, 4>(tree.scale, tree.scale, tree.scale);
            Matrix<4, 4> treeModel = treeTranslate * treeRotateY * treeScale;
            auto glTreeModel = toOpenGLMatrix(treeModel);
            glUniformMatrix4fv(objU.model, 1, GL_FALSE, glTreeModel.data());
            treeLeavesMesh->Draw();
          }

          glDisable(GL_BLEND);
        }
      }

      // -------------------------------------------------------------------
      // LANTERNAS
      // -------------------------------------------------------------------
      if (lanternMesh && !lanterns.empty()) {
        glUseProgram(lanternShader);
        applyDirectionalLight(lanternShader, moonLight, glmViewPos);
        applyPointLights(lanternShader, lanternLights);
        glUniformMatrix4fv(lanternU.view, 1, GL_FALSE, glView.data());
        glUniformMatrix4fv(lanternU.projection, 1, GL_FALSE, glProj.data());

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, lanternColorTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, lanternNormalTex);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, lanternRoughnessTex);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, lanternMetallicTex);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, lanternAOTex);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, lanternOpacityTex);

        for (const auto& lantern : lanterns) {
          Matrix<4, 4> lanternTranslate = translate<4, 4>(
              lantern.position[0], lantern.position[1], lantern.position[2]);
          Matrix<4, 4> lanternRotateY = rotateY<4, 4>(lantern.rotationY);
          Matrix<4, 4> lanternScale = scale<4, 4>(
              kLanternScale, kLanternScale, kLanternScale);
          Matrix<4, 4> lanternModel = lanternTranslate * lanternRotateY * lanternScale;
          auto glLanternModel = toOpenGLMatrix(lanternModel);
          glUniformMatrix4fv(lanternU.model, 1, GL_FALSE, glLanternModel.data());
          lanternMesh->Draw();
        }

        glDisable(GL_CULL_FACE);
      }

      glBindTexture(GL_TEXTURE_2D, skyTexture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

      // -------------------------------------------------------------------
      // CAMINHO DE TERRA
      // -------------------------------------------------------------------
      glUseProgram(pathShader);
      applyDirectionalLight(pathShader, moonLight, glmViewPos);
      applyPointLights(pathShader, lanternLights);
      glUniformMatrix4fv(pathU.view, 1, GL_FALSE, glView.data());
      glUniformMatrix4fv(pathU.projection, 1, GL_FALSE, glProj.data());
      glUniformMatrix4fv(pathU.model, 1, GL_FALSE, identity.getData());

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, dirtTexture);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, noiseTexture);
      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, dirtNormalTex);
      glActiveTexture(GL_TEXTURE3);
      glBindTexture(GL_TEXTURE_2D, dirtAOTex);
      glActiveTexture(GL_TEXTURE4);
      glBindTexture(GL_TEXTURE_2D, dirtRoughnessTex);
      glActiveTexture(GL_TEXTURE5);
      glBindTexture(GL_TEXTURE_2D, dirtDisplacementTex);

      pathMesh.Draw();

      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);

      // -------------------------------------------------------------------
      // CÉU NOTURNO
      // -------------------------------------------------------------------
      glDepthFunc(GL_LEQUAL);
      glUseProgram(skyShader);

      Matrix<4, 4> viewMatrix = cam.getViewMatrix();
      viewMatrix(0, 3) = 0.0f; // Zera translação X
      viewMatrix(1, 3) = 0.0f; // Zera translação Y
      viewMatrix(2, 3) = 0.0f; // Zera translação Z
      auto glSkyView = toOpenGLMatrix(viewMatrix);

      glUniformMatrix4fv(skyViewLoc, 1, GL_FALSE, glSkyView.data());
      glUniformMatrix4fv(skyProjLoc, 1, GL_FALSE, glProj.data());
      glUniform1i(skyTexLoc, 0);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, skyTexture);

      glBindVertexArray(skyMesh.vao);
      glDrawArrays(GL_TRIANGLES, 0, 36);
      glDepthFunc(GL_LESS);

      // -------------------------------------------------------------------
      // CURVA
      // -------------------------------------------------------------------
      if (state.showCurve) {
        glUseProgram(lineShader);
        glUniformMatrix4fv(lineU.view, 1, GL_FALSE, glView.data());
        glUniformMatrix4fv(lineU.projection, 1, GL_FALSE, glProj.data());
        glBindVertexArray(curve.vao);
        glDrawArrays(GL_LINE_STRIP, 0, curve.vertexCount);
      }
        if (state.isPlacingTroop) {
          static bool waitingForRelease = true;
          if (waitingForRelease) {
              if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
                  waitingForRelease = false;
              }
          }

          // 1. Pega a coordenada 3D do chão baseada no mouse
          Vector<3> groundPos = GetMouseGroundPos(window, cam, cameraPosition, state.fbWidth, state.fbHeight);

          // 2. Calcula a distância exata do mouse até a linha central do caminho
          float distToPath = distanceToPath(curvePoints, groundPos[0], groundPos[2]);

          // 3. Validação 1: Está EM CIMA da estrada de terra?
          // kPathHalfWidth é 2.0f. Adicionamos uma pequena folga de 0.5f para limpar o visual do caminho.
          bool isOnPath = (distToPath < (kPathHalfWidth + 0.5f));

          // 4. Validação 2: Está MUITO LONGE do caminho?
          // 12.0f cria uma faixa perfeita para colocar defensores ao longo da estrada.
          float maxAllowedDist = 12.0f;
          bool isTooFar = (distToPath > maxAllowedDist);

          // Regra solicitada: Inválido APENAS se estiver no path OU muito longe dele
          bool isInvalidPlacement = isOnPath || isTooFar;

          // 5. Define a cor do holograma (Vermelho para inválido, Azul para válido)
          glm::vec4 hColor;
          if (isInvalidPlacement) {
              hColor = glm::vec4(1.0f, 0.2f, 0.2f, 0.5f); // Vermelho translúcido
          } else {
              hColor = glm::vec4(0.2f, 0.7f, 1.0f, 0.5f); // Azul ciano translúcido
          }

          // 6. Configura o OpenGL para renderizar transparência
          glEnable(GL_BLEND);
          glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

          // 7. Ativa o shader do holograma e envia os dados
          glUseProgram(previewShader);
          glUniformMatrix4fv(glGetUniformLocation(previewShader, "view"), 1, GL_FALSE, glView.data());
          glUniformMatrix4fv(glGetUniformLocation(previewShader, "projection"), 1, GL_FALSE, glProj.data());
          glUniform4fv(glGetUniformLocation(previewShader, "previewColor"), 1, glm::value_ptr(hColor));

          // 8. Desenha o "Fantasma" na posição do mouse
          AnimatedModel* previewModel = (state.selectedTroopType == 1) ? &archerBase : &arquebusBase;

          GameObject previewGhost(previewModel, groundPos);
          previewGhost.SetAnimation("idle1");
          previewGhost.Update(deltaTime);
          previewGhost.Draw(previewShader);

          glDisable(GL_BLEND);

          // 9. Confirmação do clique esquerdo (Só permite se a área for válida!)
          ImGuiIO& io = ImGui::GetIO();
          if (!waitingForRelease && !isInvalidPlacement &&
          glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !io.WantCaptureMouse)
          {
          if (state.selectedTroopType == 1) {
              state.gold -= 50;
              GameObject newArcher(&archerBase, groundPos);
              newArcher.type = 1;
              newArcher.SetIdleAnimations({"idle1"});
              defenders.push_back(newArcher);
              defenderShoots.push_back(DefenderShoot{0.0f, false});
          }
          else if (state.selectedTroopType == 2) {
              state.gold -= 75;
              GameObject newArquebus(&arquebusBase, groundPos);
              newArquebus.type = 2;
              newArquebus.SetIdleAnimations({"idle1"});
              defenders.push_back(newArquebus);
              defenderShoots.push_back(DefenderShoot{0.0f, false});
          }

          state.isPlacingTroop = false;
          waitingForRelease = true;
          }

          // 10. Cancelar posicionamento com botão direito
          if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
              state.isPlacingTroop = false;
              waitingForRelease = true;
          }
      }
      float currentFps = 1.0f / deltaTime;
      gameHud.Render(state, currentFps);

      // if (hasReachedEnd) {
      //   break;
      // }

      glfwSwapBuffers(window);
      glfwPollEvents();
    }

    // ---------------------------------------------------------------------
    // LIMPEZA DE RECURSOS GL
    // ---------------------------------------------------------------------
    gameHud.Shutdown();
    glDeleteTextures(1, &grassTexture);
    glDeleteTextures(1, &grassNormalTex);
    glDeleteTextures(1, &grassAOTex);
    glDeleteTextures(1, &grassRoughnessTex);
    glDeleteTextures(1, &grassDisplacementTex);
    glDeleteTextures(1, &dirtTexture);
    glDeleteTextures(1, &dirtNormalTex);
    glDeleteTextures(1, &dirtAOTex);
    glDeleteTextures(1, &dirtRoughnessTex);
    glDeleteTextures(1, &dirtDisplacementTex);
    glDeleteTextures(1, &noiseTexture);
    glDeleteTextures(1, &archerTexture);
    glDeleteTextures(1, &treeLeavesTexture);
    glDeleteTextures(1, &treeLogTexture);
    glDeleteTextures(1, &lanternColorTex);
    glDeleteTextures(1, &lanternNormalTex);
    glDeleteTextures(1, &lanternRoughnessTex);
    glDeleteTextures(1, &lanternMetallicTex);
    glDeleteTextures(1, &lanternAOTex);
    glDeleteTextures(1, &lanternOpacityTex);
    glDeleteTextures(1, &castleTex);
    glDeleteTextures(1, &castleNormalTex);

    glDeleteVertexArrays(1, &grass.vao);
    glDeleteBuffers(1, &grass.vbo);
    glDeleteVertexArrays(1, &curve.vao);
    glDeleteBuffers(1, &curve.vbo);

    glDeleteProgram(groundShader);
    glDeleteProgram(objShader);
    glDeleteProgram(pathShader);
    glDeleteProgram(lineShader);
    glDeleteProgram(shaderAnim);
    glDeleteProgram(lanternShader);
    return 0;
  }

}  // namespace

int main() {
  if (!glfwInit()) {
    return 1;
  }
  srand(static_cast<unsigned int>(time(NULL)));
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

  const int exitCode = run(window);

  glfwDestroyWindow(window);
  glfwTerminate();
  return exitCode;
}
