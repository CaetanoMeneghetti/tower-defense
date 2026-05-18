#pragma once

#include <string>
#include <vector>

// =============================================================================
// CARREGADOR DE OBJ ESTÁTICO
// =============================================================================
// Estruturas de dados para o vértice do OBJ.

struct Vec2 {
  float x, y;
};

struct Vec3 {
  float x, y, z;
};

struct Vertex {
  Vec3 position;    // posição 3d do ponto
  Vec2 texcoords;   // mapeamento da imagem para o vértice (0.0-1.0)
  Vec3 normal;      // vetor normal do vértice
};

// Lê um arquivo OBJ (apenas v/vt/vn/f triangular). Centraliza a mesh em XZ e
// fixa o pé em Y=0. Retorna false se o arquivo não pôde ser aberto.
bool loadObj(const std::string &path, std::vector<Vertex> &outVertices);
