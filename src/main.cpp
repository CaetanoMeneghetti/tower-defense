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

  struct AppState {
    // Modo de câmera
    bool useFreeCamera = false;
    bool cPressed = false;
    bool showCurve = false;
    bool tPressed = false;

    // Câmera livre
    float yaw = -math_constants::kHalfPi;
    float pitch = 0.0f;
    float lastX = kWindowWidth / 2.0f;
    float lastY = kWindowHeight / 2.0f;
    bool firstMouse = true;

    // Câmera orbital
    float orbitYaw = 0.0f;
    float orbitPitch = math_constants::kPi / 6.0f;
    float orbitRadius = 10.0f;

    // Tamanho atual do framebuffer (atualizado pelo resize callback)
    int fbWidth = kWindowWidth;
    int fbHeight = kWindowHeight;
  };

  struct GroundUniforms {
    GLint view;
    GLint projection;
    GLint grass;
    GLint noise;

    GroundUniforms(GLint v, GLint p, GLint g, GLint n)
      : view(v), projection(p), grass(g), noise(n) {}
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

    PathUniforms(GLint v, GLint p, GLint m, GLint d, GLint n)
    : view(v), projection(p), model(m), dirt(d), noise(n) {}
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

  // Cria um plano [-20, 20] no XZ com UVs, usado como chão de grama.
  GpuMesh createGrassMesh() {
    GpuMesh m;
    // clang-format off
    static const float vertices[] = {
    // X,     Y,     Z,        U,      V
      // Primeiro triângulo
      -20.0f, 0.0f, -20.0f,     0.0f,  20.0f,  // Trás esquerda
       20.0f, 0.0f, -20.0f,    20.0f,  20.0f,  // Trás direita
      -20.0f, 0.0f,  20.0f,     0.0f,   0.0f,  // Frente esquerda
      // Segundo triângulo
       20.0f, 0.0f, -20.0f,    20.0f,  20.0f,  // Trás direita
       20.0f, 0.0f,  20.0f,    20.0f,   0.0f,  // Frente direita
      -20.0f, 0.0f,  20.0f,     0.0f,   0.0f,  // Frente esquerda
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
    outAngle = std::atan2(dx, dy);

    return {p1.x + dx * t, 0.1f, p1.y + dy * t};
  }

  // =========================================================================
  // EXECUÇÃO PRINCIPAL
  // =========================================================================
  // Tudo que aloca recurso GL vive aqui dentro. Quando run() retorna, todos
  // os recursos foram liberados e os destrutores de Mesh já rodaram, ainda
  // com o contexto GL válido. Só depois main() chama glfwDestroyWindow
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

    // ---------------------------------------------------------------------
    // SHADERS
    // ---------------------------------------------------------------------
    // groundShader: chão de grama
    // objShader:    entidades
    // pathShader:   caminho de terra
    // lineShader:   curva-guia amarela
    unsigned int groundShader = createShaderProgram("data/shaders/grass.vert", "data/shaders/grass.frag");
    unsigned int objShader = createShaderProgram("data/shaders/shader.vert", "data/shaders/shader.frag");
    unsigned int pathShader = createShaderProgram("data/shaders/path.vert", "data/shaders/path.frag");
    unsigned int lineShader = createShaderProgram("data/shaders/line.vert", "data/shaders/line.frag");

    if (!groundShader || !objShader || !pathShader || !lineShader) {
      std::cout << "ERRO: Falha ao criar um ou mais shaders" << std::endl;
      glDeleteProgram(groundShader);
      glDeleteProgram(objShader);
      glDeleteProgram(pathShader);
      glDeleteProgram(lineShader);
      return 1;
    }

    // Cache das uniform locations
    GroundUniforms groundU{
        glGetUniformLocation(groundShader, "view"),
        glGetUniformLocation(groundShader, "projection"),
        glGetUniformLocation(groundShader, "grass"),
        glGetUniformLocation(groundShader, "noise"),
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
    };
    LineUniforms lineU{
        glGetUniformLocation(lineShader, "view"),
        glGetUniformLocation(lineShader, "projection"),
    };

    // Texture units são constantes — setamos uma vez só (era refeito todo frame).
    glUseProgram(groundShader);
    glUniform1i(groundU.grass, 0);
    glUniform1i(groundU.noise, 1);
    glUniform1i(glGetUniformLocation(groundShader, "normalMap"),       2);
    glUniform1i(glGetUniformLocation(groundShader, "aoMap"),           3);
    glUniform1i(glGetUniformLocation(groundShader, "roughnessMap"),    4);
    glUniform1i(glGetUniformLocation(groundShader, "displacementMap"), 5);

    glUseProgram(pathShader);
    glUniform1i(pathU.dirt, 0);
    glUniform1i(pathU.noise, 1);
    glUniform1i(glGetUniformLocation(pathShader, "normalMap"),       2);
    glUniform1i(glGetUniformLocation(pathShader, "aoMap"),           3);
    glUniform1i(glGetUniformLocation(pathShader, "roughnessMap"),    4);
    glUniform1i(glGetUniformLocation(pathShader, "displacementMap"), 5);

    // ---------------------------------------------------------------------
    // CARREGA OBJ ANTES DE ALOCAR MAIS RECURSOS GL
    // ---------------------------------------------------------------------
    std::vector<Vertex> objVertices;
    if (!Parser("data/models/test.obj", objVertices)) {
      std::cout << "ERRO: Nao encontrou data/models/test.obj" << std::endl;
      glDeleteProgram(groundShader);
      glDeleteProgram(objShader);
      glDeleteProgram(pathShader);
      glDeleteProgram(lineShader);
      return 1;
    }

    std::vector<Vertex> bowVertices;
    if (!Parser("data/models/Archer/bow.obj", bowVertices)) {
      std::cout << "ERRO: Nao encontrou data/models/Archer/bow.obj" << std::endl;
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

    Mesh pathMesh = generatePathMesh(curvePoints, 2.0f);
    Mesh testMesh(objVertices);
    Mesh bowMesh(bowVertices);

    // ---------------------------------------------------------------------
    // TEXTURAS
    // ---------------------------------------------------------------------
    unsigned int grassTexture        = loadTexture("data/textures/grass_color.png");
    unsigned int grassNormalTex      = loadTexture("data/textures/grass_normal.png");
    unsigned int grassAOTex          = loadTexture("data/textures/grass_ambient_occlusion.png");
    unsigned int grassRoughnessTex   = loadTexture("data/textures/grass_roughness.png");
    unsigned int grassDisplacementTex= loadTexture("data/textures/grass_displacement.png");
    unsigned int dirtTexture          = loadTexture("data/textures/dirt_color.png");
    unsigned int dirtNormalTex        = loadTexture("data/textures/dirt_normal.png");
    unsigned int dirtAOTex            = loadTexture("data/textures/dirt_ambient_occlusion.png");
    unsigned int dirtRoughnessTex     = loadTexture("data/textures/dirt_roughness.png");
    unsigned int dirtDisplacementTex  = loadTexture("data/textures/dirt_displacement.png");
    unsigned int noiseTexture        = loadTexture("data/textures/perlin_noise.jpg", 3);
    unsigned int archerTexture       = loadTexture("data/textures/archer.png");
    unsigned int bowTexture          = loadTexture("data/textures/bow.jpg");

    // ---------------------------------------------------------------------
    // MOVIMENTAÇÃO DO PERSONAGEM
    // ---------------------------------------------------------------------
    float characterDistance = 0.0f;
    const float characterSpeed = 2.0f;

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


    defenders.push_back(GameObject(&archerBase, Vector<3>{ 0.0f, 0.1f, 0.0f }));

    defenders.back().SetIdleAnimations({"aim"});

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
    while (!glfwWindowShouldClose(window)) {
      double currentTime = glfwGetTime();

      if (currentTime - lastTime < frameDelay) {
          continue;
      }

      const float deltaTime = static_cast<float>(frameDelay);

      for (auto& unit : defenders) {
          unit.Update(deltaTime);
      }


      lastTime += frameDelay;

      characterDistance += characterSpeed * deltaTime;

      processInput(window, cameraPosition, deltaTime);

      // Céu noturno — combina com o ambient azulado da lua
      glClearColor(0.015f, 0.018f, 0.045f, 1.0f);
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
      glUseProgram(objShader);
      applyDirectionalLight(objShader, moonLight, glmViewPos);
      glUniformMatrix4fv(objU.view, 1, GL_FALSE, glView.data());
      glUniformMatrix4fv(objU.projection, 1, GL_FALSE, glProj.data());

      float characterAngle = 0.0f;
      bool hasReachedEnd = false;
      Vector<3> characterPos = getPositionAtDistance(curvePoints, curveCache, characterDistance, characterAngle, hasReachedEnd);
      Matrix<4, 4> characterTranslate = translate<4, 4>(characterPos[0], characterPos[1], characterPos[2]);
      Matrix<4, 4> characterRotateY = rotateY<4, 4>(characterAngle);
      Matrix<4, 4> characterRotateX = rotateX<4, 4>(-math_constants::kHalfPi);
      Matrix<4, 4> characterModel = characterTranslate * characterRotateY * characterRotateX;

      auto glModel = toOpenGLMatrix(characterModel);
      glUniformMatrix4fv(objU.model, 1, GL_FALSE, glModel.data());
      testMesh.Draw();
      glUseProgram(shaderAnim);
      applyDirectionalLight(shaderAnim, moonLight, glmViewPos);
      glUniformMatrix4fv(glGetUniformLocation(shaderAnim, "view"), 1, GL_FALSE, glView.data());
      glUniformMatrix4fv(glGetUniformLocation(shaderAnim, "projection"), 1, GL_FALSE, glProj.data());


      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, archerTexture);
      glUniform1i(glGetUniformLocation(shaderAnim, "tex"), 0);

      for (auto& unit : defenders) {
          unit.Draw(shaderAnim);
      }
      glUseProgram(objShader);
      applyDirectionalLight(objShader, moonLight, glmViewPos);
      glUniformMatrix4fv(objU.view, 1, GL_FALSE, glView.data());
      glUniformMatrix4fv(objU.projection, 1, GL_FALSE, glProj.data());

      for (auto& unit : defenders) {

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

      // -------------------------------------------------------------------
      // CHÃO DE GRAMA
      // -------------------------------------------------------------------
      glUseProgram(groundShader);
      applyDirectionalLight(groundShader, moonLight, glmViewPos);
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
      // CAMINHO DE TERRA
      // -------------------------------------------------------------------
      glUseProgram(pathShader);
      applyDirectionalLight(pathShader, moonLight, glmViewPos);
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
      // 4. CURVA
      // -------------------------------------------------------------------
      if (state.showCurve) {
        glUseProgram(lineShader);
        glUniformMatrix4fv(lineU.view, 1, GL_FALSE, glView.data());
        glUniformMatrix4fv(lineU.projection, 1, GL_FALSE, glProj.data());
        glBindVertexArray(curve.vao);
        glDrawArrays(GL_LINE_STRIP, 0, curve.vertexCount);
      }

      if (hasReachedEnd) {
        break;
      }

      glfwSwapBuffers(window);
      glfwPollEvents();
    }

    // ---------------------------------------------------------------------
    // LIMPEZA DE RECURSOS GL
    // ---------------------------------------------------------------------
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

    glDeleteVertexArrays(1, &grass.vao);
    glDeleteBuffers(1, &grass.vbo);
    glDeleteVertexArrays(1, &curve.vao);
    glDeleteBuffers(1, &curve.vbo);

    glDeleteProgram(groundShader);
    glDeleteProgram(objShader);
    glDeleteProgram(pathShader);
    glDeleteProgram(lineShader);
    glDeleteProgram(shaderAnim);
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
