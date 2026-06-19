#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// =============================================================================
// CARREGADOR DE OBJ ESTÁTICO
// =============================================================================

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

// Variante multi-material: respeita os 'usemtl' do OBJ e retorna um grupo
// por material. Aplica centralização global em XZ e pé em Y=0 consistente
// entre todos os grupos. Retorna false se o arquivo não pôde ser aberto.
struct ObjMaterialGroup {
  std::string         material;
  std::vector<Vertex> vertices;
};
bool loadObjMaterials(const std::string &path, std::vector<ObjMaterialGroup> &out);

// =============================================================================
// CARREGADOR DE MTL
// =============================================================================

struct MtlMaterial {
  std::string map_Kd;              // caminho da textura difusa (relativo ao .mtl)
  Vec3        Kd = {1.0f, 1.0f, 1.0f};  // cor difusa base
};

// Lê um arquivo .mtl e preenche 'out' com nome_do_material → MtlMaterial.
// Retorna false se o arquivo não pôde ser aberto.
bool loadMtl(const std::string &path,
             std::unordered_map<std::string, MtlMaterial> &out);

// Lê a diretiva 'mtllib' de um arquivo OBJ, resolve o caminho relativo ao
// diretório do OBJ e retorna os materiais parseados com map_Kd já resolvido
// para caminho relativo ao diretório do OBJ (pronto para loadTexture).
// Retorna mapa vazio se não houver mtllib ou o arquivo não puder ser aberto.
std::unordered_map<std::string, MtlMaterial>
parseMtlFromObj(const std::string &objPath);
