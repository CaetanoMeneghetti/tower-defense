#include "engine/catmull_rom.h"

#include <cmath>

namespace {
  inline float dist(const Point &a, const Point &b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
  }
}  // namespace

std::vector<Point> generateCatmullRomVertices(const std::vector<Point> &controlPoints,
                                              int segmentsPerCurve) {
  std::vector<Point> vertices;
  int n = controlPoints.size();

  if (n < 2) {
    return controlPoints;
  }

  bool isClosed = dist(controlPoints.front(), controlPoints.back()) < 1e-4f;

  for (int i = 0; i < n - 1; ++i) {
    Point p0;
    Point p1 = controlPoints[i];
    Point p2 = controlPoints[i + 1];
    Point p3;

    // Se estamos no primeiro segmento (i == 0),
    // não existe um ponto anterior real (controlPoints[-1]).
    if (i == 0) {
      // Cria-se um ponto anterior virtual
      // p0 = p1 - (p2 - p1)
      p0 = Point{p1.x - (p2.x - p1.x), p1.y - (p2.y - p1.y)};
    } else {
      p0 = controlPoints[i - 1];
    }
    // Se estamos no último segmento (i == n - 2),
    // não existe um ponto seguinte real (controlPoints[n]).
    if (i == n - 2) {
      // Cria-se um ponto posterior virtual
      // p3 = p2 + (p2 - p1)
      p3 = Point{p2.x + (p2.x - p1.x), p2.y + (p2.y - p1.y)};
    } else {
      p3 = controlPoints[i + 2];
    }

    for (int step = 0; step < segmentsPerCurve; ++step) {
      float t = (float)step / segmentsPerCurve;
      float t2 = t * t;
      float t3 = t2 * t;

      Point p;

      // [https://pt.scribd.com/document/331142118/Catmull-Rom-Spline]
      p.x = 0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                    (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                    (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);

      p.y = 0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                    (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                    (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);

      vertices.push_back(p);
    }
  }

  vertices.push_back(controlPoints.back());

  return vertices;
}
