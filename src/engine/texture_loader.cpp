#include "engine/texture_loader.h"

#include <glad/glad.h>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

unsigned int loadTexture(const char *path, int forcedChannels) {
  unsigned int tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Alinhamento 1 cobre texturas com largura "estranha" (alguns JPGs).
  // Restauramos para 4 ao final para não afetar uploads futuros.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  int w, h, channels;
  unsigned char *data = stbi_load(path, &w, &h, &channels, forcedChannels);

  if (!data) {
    std::cout << "ERRO: Falha ao carregar a textura " << path << std::endl;
    glDeleteTextures(1, &tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    return 0;
  }

  const int actualChannels = (forcedChannels != 0) ? forcedChannels : channels;
  GLenum format;
  switch (actualChannels) {
    case 1:  format = GL_RED;  break;
    case 2:  format = GL_RG;   break;
    case 4:  format = GL_RGBA; break;
    default: format = GL_RGB;  break;
  }

  glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);
  stbi_image_free(data);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  return tex;
}

unsigned int createDefaultNormalTexture() {
  unsigned char flatNormal[3] = {128, 128, 255};
  unsigned int tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, flatNormal);
  return tex;
}
