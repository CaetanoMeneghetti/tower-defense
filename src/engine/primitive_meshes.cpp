#include "engine/primitive_meshes.h"

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
       1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f,
  };

  glGenVertexArrays(1, &m.vao);
  glGenBuffers(1, &m.vbo);
  glBindVertexArray(m.vao);
  glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  m.vertexCount = 36;
  return m;
}

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
  glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  m.vertexCount = static_cast<GLsizei>(curvePoints.size());
  return m;
}

void deleteGpuMesh(GpuMesh &mesh) {
  if (mesh.vao != 0) glDeleteVertexArrays(1, &mesh.vao);
  if (mesh.vbo != 0) glDeleteBuffers(1, &mesh.vbo);
  mesh.vao = 0;
  mesh.vbo = 0;
  mesh.vertexCount = 0;
}
