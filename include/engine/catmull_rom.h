#pragma once

#include <cmath>
#include <vector>

struct Point {
  float x;
  float y;

  float length() const { return std::sqrt(x * x + y * y); }
};

inline float dist(const Point &a, const Point &b) {
  return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

std::vector<Point> generateCatmullRomVertices(const std::vector<Point> &controlPoints,
                                              int segmentsPerCurve = 20);
