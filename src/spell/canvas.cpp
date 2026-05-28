#include "spell/canvas.h"

#include <cstdlib>

namespace spell {

void clear(Canvas &c) {
  c.pixels.fill(1.0f);
}

static void setPixel(Canvas &c, int x, int y) {
  if (x < 0 || y < 0 || x >= kCanvasSize || y >= kCanvasSize) return;
  c.pixels[y * kCanvasSize + x] = 0.0f;
}

void plot(Canvas &c, int x, int y) {
  setPixel(c, x, y);
  setPixel(c, x - 1, y);
  setPixel(c, x + 1, y);
  setPixel(c, x, y - 1);
  setPixel(c, x, y + 1);
}

void drawLine(Canvas &c, int x0, int y0, int x1, int y1) {
  int dx = std::abs(x1 - x0);
  int dy = -std::abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    plot(c, x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

}  // namespace spell
