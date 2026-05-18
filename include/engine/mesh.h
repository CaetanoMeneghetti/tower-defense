#pragma once

#include <vector>

#include "engine/obj_loader.h"

// =============================================================================
// MESH ESTÁTICA (sem skinning)
// =============================================================================
// Usada para OBJs estáticos (castelo, árvores, lanternas, armas). Cada Mesh
// detém seu VAO/VBO; o destrutor não libera (legado — gerencie via run()).
class Mesh {
 public:
  std::vector<Vertex> vertices;
  unsigned int vao = 0;
  unsigned int vbo = 0;

  Mesh(std::vector<Vertex> vertices);
  void draw();

 private:
  void setupMesh();
};
