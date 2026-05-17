#include "engine/parser.h"

#include <fstream>
#include <iostream>
#include <sstream>

bool Parser(const std::string &path, std::vector<Vertex> &out_vertices) {
  // Listas temporárias pra guardar os dados do .obj
  std::vector<Vec3> temp_positions;
  std::vector<Vec2> temp_texCoords;
  std::vector<Vec3> temp_normals;

  std::ifstream file(path);
  if (!file.is_open()) return false;

  std::string lineHeader;
  while (file >> lineHeader) {
    if (lineHeader == "v") {
      Vec3 pos;
      file >> pos.x >> pos.y >> pos.z;
      temp_positions.push_back(pos);
    } else if (lineHeader == "vt") {
      Vec2 tex;
      file >> tex.x >> tex.y;
      tex.y = 1.0f - tex.y;
      temp_texCoords.push_back(tex);
    } else if (lineHeader == "vn") {
      Vec3 norm;
      file >> norm.x >> norm.y >> norm.z;
      temp_normals.push_back(norm);
    } else if (lineHeader == "f") {
      for (int i = 0; i < 3; i++) {
        std::string vertexStr;
        file >> vertexStr;

        // 1/1/1 -> 1 1 1
        for (char &c : vertexStr)
          if (c == '/') c = ' ';

        std::stringstream ss(vertexStr);
        int vIdx, vtIdx, vnIdx;
        ss >> vIdx >> vtIdx >> vnIdx;

        Vertex v;
        v.position = temp_positions[vIdx - 1];
        v.texcoords = temp_texCoords[vtIdx - 1];
        v.normal = temp_normals[vnIdx - 1];

        out_vertices.push_back(v);
      }
    }
  }

  // Se carregou vértices, centraliza a malha no eixo X e Z,
  // e colocar os pés do modelo no Y = 0
  if (!out_vertices.empty()) {
    float minX = out_vertices[0].position.x, maxX = out_vertices[0].position.x;
    float minY = out_vertices[0].position.y, maxY = out_vertices[0].position.y;
    float minZ = out_vertices[0].position.z, maxZ = out_vertices[0].position.z;

    // Delimita os limites de um cubo ao redor
    for (const auto& v : out_vertices) {
      if (v.position.x < minX) minX = v.position.x;
      if (v.position.x > maxX) maxX = v.position.x;
      if (v.position.y < minY) minY = v.position.y;
      if (v.position.y > maxY) maxY = v.position.y;
      if (v.position.z < minZ) minZ = v.position.z;
      if (v.position.z > maxZ) maxZ = v.position.z;
    }

    float centerX = (minX + maxX) / 2.0f;
    float centerZ = (minZ + maxZ) / 2.0f;

    // Coloca o centro de X e Z em 0
    // Coloca o mínimo de Y em 0
    for (auto& v : out_vertices) {
      v.position.x -= centerX;
      v.position.y -= minY;
      v.position.z -= centerZ;
    }
  }

  return true;
}
