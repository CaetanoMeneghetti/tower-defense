#pragma once

#include <vector>

struct Point {
  float x;
  float y;
};

std::vector<Point> generateCatmullRomVertices(const std::vector<Point> &controlPoints,
                                              int segmentsPerCurve = 20);
