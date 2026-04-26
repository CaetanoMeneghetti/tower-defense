#include "engine/parser.h"
#include <fstream>
#include <sstream>
#include <iostream>

bool Parser(const std::string& path, std::vector<Vertex>& out_vertices)
{
    //listas temp pra guardar os dados do .obj separado
    std::vector<Vec3>temp_positions;
    std::vector<Vec2>temp_texCoords;
    std::vector<Vec3>temp_normals;

    std::ifstream file(path);
    if(!file.is_open()) return false; 

    std::string lineHeader;
    while (file >> lineHeader)
    {
        if (lineHeader == "v") //Vertex: pos
        {
            Vec3 pos;
            file >> pos.x >> pos.y >> pos.z;
            temp_positions.push_back(pos);
        }
        else if (lineHeader == "vt")//vertex: cord do mapeamento de texturas
        {
            Vec2 tex;
            file >> tex.x >> tex.y;
            temp_texCoords.push_back(tex);
        }
        else if (lineHeader == "vn") //vertex: normais
        {
            Vec3 norm;
            file >> norm.x >> norm.y >> norm.z;
            temp_normals.push_back(norm);
        }
        else if (lineHeader == "f") //junta tudo
        {
            for (int i = 0; i < 3; i++)
            {
                std::string vertexStr;
                file >> vertexStr;

                for (char &c : vertexStr) if (c == '/') c = ' '; //1/1/1 -> 1 1 1 pro stringstream
                
                std::stringstream ss(vertexStr);
                int vIdx, vtIdx, vnIdx;
                ss >> vIdx >> vtIdx >> vnIdx;

                Vertex v;
                v.position  = temp_positions[vIdx - 1];
                v.texcoords = temp_texCoords[vtIdx - 1];
                v.normal    = temp_normals[vnIdx - 1];

                out_vertices.push_back(v);
            }
        }
    }

    return true;
}