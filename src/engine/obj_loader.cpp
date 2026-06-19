#include "engine/obj_loader.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

bool loadObj(const std::string &path, std::vector<Vertex> &out_vertices) {
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
      // Lê a linha toda da face — pode ter 3 (tri), 4 (quad) ou mais
      // vértices (n-gono). Triangulamos como fan a partir de v0:
      //   (v0, v1, v2), (v0, v2, v3), (v0, v3, v4), ...
      // Sem isso, OBJs com quads perdem metade dos triângulos.
      std::string restOfLine;
      std::getline(file, restOfLine);
      std::stringstream lineStream(restOfLine);

      std::vector<Vertex> faceVertices;
      std::string vertexStr;
      while (lineStream >> vertexStr) {
        // 1/1/1 -> 1 1 1
        for (char &c : vertexStr) {
          if (c == '/') c = ' ';
        }

        std::stringstream tokenStream(vertexStr);
        int vIdx, vtIdx, vnIdx;
        tokenStream >> vIdx >> vtIdx >> vnIdx;

        Vertex v;
        v.position = temp_positions[vIdx - 1];
        v.texcoords = temp_texCoords[vtIdx - 1];
        v.normal = temp_normals[vnIdx - 1];
        faceVertices.push_back(v);
      }

      // Triangulação em fan
      for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
        out_vertices.push_back(faceVertices[0]);
        out_vertices.push_back(faceVertices[i]);
        out_vertices.push_back(faceVertices[i + 1]);
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

// =============================================================================
// MTL
// =============================================================================

// Lê o resto da linha após o token atual, pula flags (-s, -bm, etc.) e
// retorna o último token — que é o caminho do arquivo.
// Lida com espaços no path (ex: "C:/Users/Fernando Tedesco/...").
static std::string readMapPath(std::istream &file) {
  std::string line;
  std::getline(file, line);

  // Divide em tokens manualmente
  std::vector<std::string> tokens;
  std::istringstream ss(line);
  std::string tok;
  while (ss >> tok) tokens.push_back(tok);

  if (tokens.empty()) return {};

  // Pula flags (-s, -bm, -o, etc.) e seus argumentos numéricos
  size_t i = 0;
  while (i < tokens.size()) {
    if (tokens[i].empty()) { ++i; continue; }
    if (tokens[i][0] == '-') {
      ++i;  // pula o nome da flag
      // pula os argumentos numéricos da flag
      while (i < tokens.size() && !tokens[i].empty() && tokens[i][0] != '-') {
        // se parece número, é argumento da flag
        char *end;
        std::strtod(tokens[i].c_str(), &end);
        if (end != tokens[i].c_str() && *end == '\0') { ++i; continue; }
        break;  // não é número: começa o path
      }
    } else {
      break;  // primeiro token não-flag é o início do path
    }
  }

  if (i >= tokens.size()) return {};

  // O que sobrou pode ter espaços (path com espaço) — junta de volta
  std::string result = tokens[i];
  for (size_t j = i + 1; j < tokens.size(); ++j) result += ' ' + tokens[j];
  return result;
}

bool loadMtl(const std::string &path,
             std::unordered_map<std::string, MtlMaterial> &out) {
  std::ifstream file(path);
  if (!file.is_open()) return false;

  std::string current;
  std::string token;
  while (file >> token) {
    if (token == "newmtl") {
      file >> current;
      out.emplace(current, MtlMaterial{});
    } else if (token == "map_Kd" && !current.empty()) {
      out[current].map_Kd = readMapPath(file);
    } else if (token == "Kd" && !current.empty()) {
      MtlMaterial &m = out[current];
      file >> m.Kd.x >> m.Kd.y >> m.Kd.z;
    }
    // tokens desconhecidos: próxima iteração consome a próxima palavra naturalmente
  }
  return true;
}

std::unordered_map<std::string, MtlMaterial>
parseMtlFromObj(const std::string &objPath) {
  std::string dir;
  const size_t slash = objPath.find_last_of("/\\");
  if (slash != std::string::npos) dir = objPath.substr(0, slash + 1);

  std::ifstream file(objPath);
  std::unordered_map<std::string, MtlMaterial> result;
  if (!file.is_open()) return result;

  std::string token;
  while (file >> token) {
    if (token == "mtllib") {
      std::string mtlFile;
      file >> mtlFile;
      loadMtl(dir + mtlFile, result);
      break;
    }
  }

  // Resolve map_Kd relativo ao diretório do OBJ (pronto pra loadTexture)
  for (auto &[name, mat] : result) {
    if (mat.map_Kd.empty()) continue;
    const bool isAbsolute = (mat.map_Kd[0] == '/' || mat.map_Kd[0] == '\\')
                         || (mat.map_Kd.size() > 1 && mat.map_Kd[1] == ':');
    if (!isAbsolute) mat.map_Kd = dir + mat.map_Kd;
  }

  return result;
}

bool loadObjMaterials(const std::string &path, std::vector<ObjMaterialGroup> &out) {
  std::vector<Vec3> temp_positions;
  std::vector<Vec2> temp_texCoords;
  std::vector<Vec3> temp_normals;

  std::ifstream file(path);
  if (!file.is_open()) return false;

  std::string currentMaterial = "__default__";
  std::unordered_map<std::string, size_t> matIndex;

  auto getGroup = [&](const std::string &name) -> ObjMaterialGroup & {
    auto it = matIndex.find(name);
    if (it == matIndex.end()) {
      matIndex[name] = out.size();
      out.push_back({name, {}});
      return out.back();
    }
    return out[it->second];
  };

  std::string token;
  while (file >> token) {
    if (token == "v") {
      Vec3 pos;
      file >> pos.x >> pos.y >> pos.z;
      temp_positions.push_back(pos);
    } else if (token == "vt") {
      Vec2 tex;
      file >> tex.x >> tex.y;
      tex.y = 1.0f - tex.y;
      temp_texCoords.push_back(tex);
    } else if (token == "vn") {
      Vec3 norm;
      file >> norm.x >> norm.y >> norm.z;
      temp_normals.push_back(norm);
    } else if (token == "usemtl") {
      file >> currentMaterial;
      getGroup(currentMaterial);
    } else if (token == "f") {
      std::string restOfLine;
      std::getline(file, restOfLine);
      std::stringstream lineStream(restOfLine);

      std::vector<Vertex> faceVertices;
      std::string vertexStr;
      while (lineStream >> vertexStr) {
        for (char &c : vertexStr) if (c == '/') c = ' ';
        std::stringstream tokenStream(vertexStr);
        int vIdx, vtIdx, vnIdx;
        tokenStream >> vIdx >> vtIdx >> vnIdx;
        Vertex v;
        v.position  = temp_positions[vIdx  - 1];
        v.texcoords = temp_texCoords[vtIdx - 1];
        v.normal    = temp_normals  [vnIdx - 1];
        faceVertices.push_back(v);
      }
      ObjMaterialGroup &g = getGroup(currentMaterial);
      for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
        g.vertices.push_back(faceVertices[0]);
        g.vertices.push_back(faceVertices[i]);
        g.vertices.push_back(faceVertices[i + 1]);
      }
    }
  }

  if (out.empty()) return true;

  // Bounding box global (todos os grupos compartilham o mesmo espaço)
  float minX, maxX, minY, minZ, maxZ;
  bool first = true;
  for (const auto &g : out) {
    for (const auto &v : g.vertices) {
      if (first) {
        minX = maxX = v.position.x;
        minY        = v.position.y;
        minZ = maxZ = v.position.z;
        first = false;
      } else {
        if (v.position.x < minX) minX = v.position.x;
        if (v.position.x > maxX) maxX = v.position.x;
        if (v.position.y < minY) minY = v.position.y;
        if (v.position.z < minZ) minZ = v.position.z;
        if (v.position.z > maxZ) maxZ = v.position.z;
      }
    }
  }

  const float cx = (minX + maxX) / 2.0f;
  const float cz = (minZ + maxZ) / 2.0f;
  for (auto &g : out)
    for (auto &v : g.vertices) {
      v.position.x -= cx;
      v.position.y -= minY;
      v.position.z -= cz;
    }

  return true;
}

