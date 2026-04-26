#pragma once
#include <vector>
#include "parser.h"

class Mesh {
public:
    std::vector<Vertex> vertices;
    unsigned int VAO, VBO;

    Mesh(std::vector<Vertex> vertices);
    void Draw();

private:
    void setupMesh();
};