#pragma once

#include <array>

namespace spell {

constexpr int kCanvasSize = 50;
constexpr int kCanvasPixels = kCanvasSize * kCanvasSize;

// Buffer 50x50 em [0,1]. Background = 1.0 (branco), traço = 0.0 (preto).
// A inversão para o formato do modelo (white-on-black) acontece no classifier.
struct Canvas {
  std::array<float, kCanvasPixels> pixels;
};

void clear(Canvas &c);

// Pinta um pixel + 4 vizinhos (cross 3x3) para dar grossura mínima ao traço.
void plot(Canvas &c, int x, int y);

// Linha entre dois pontos no espaço do canvas (Bresenham + grossura via plot).
void drawLine(Canvas &c, int x0, int y0, int x1, int y1);

}  // namespace spell
