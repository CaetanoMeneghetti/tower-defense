#pragma once

// =============================================================================
// CARREGAMENTO DE TEXTURA (STB_IMAGE)
// =============================================================================
// Carrega uma textura 2D RGBA/RGB/RG/R do disco, gera mipmaps e retorna o
// handle do OpenGL. Retorna 0 em caso de falha.
// `forcedChannels = 0` deixa a stb_image decidir; valores > 0 forçam a leitura
// em N canais (útil para PNGs com alpha que precisam virar RGBA).
unsigned int loadTexture(const char *path, int forcedChannels = 0);

// Textura 1x1 neutra para fallback de normal map (RGB = 128,128,255 → normal Z+).
unsigned int createDefaultNormalTexture();
