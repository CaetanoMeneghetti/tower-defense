#pragma once

#include <vector>

#include "engine/catmull_rom.h"
#include "engine/mesh.h"

Mesh generatePathMesh(const std::vector<Point> &centerPoints, float baseWidth);
