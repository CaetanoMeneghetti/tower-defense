#pragma once
#include <vector>
#include <string>

//estruturas de dados necessárias

struct Vec2
{
    float x,y;
};

struct Vec3
{
    float x,y,z;
};

struct Vertex
{
    Vec3 position; //pos 3d do ponto
    Vec2 texcoords;//info de mapeamento da imagem pro vértice (imagem->ponto de uma face, float 0.0-1.0)
    Vec3 normal; //vetor de direção do vértice
};

bool Parser(const std::string& path, std::vector<Vertex>& out_vertices); //caminho do obj e onde guardar



