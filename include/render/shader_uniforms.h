#pragma once

#include <glad/glad.h>

// =============================================================================
// CACHES DE UNIFORM LOCATIONS POR SHADER
// =============================================================================
// Resolver glGetUniformLocation toda hora é caro. Estes structs cacheiam as
// localizações na construção e os draws apenas consultam o campo.

struct GroundUniforms {
  GLint view;
  GLint projection;
  GLint grass;
  GLint noise;
  GLint fogColor;
  GLint fogStart;
  GLint fogEnd;
};

struct ObjUniforms {
  GLint view;
  GLint projection;
  GLint model;
};

struct PathUniforms {
  GLint view;
  GLint projection;
  GLint model;
  GLint dirt;
  GLint noise;
  GLint fogColor;
  GLint fogStart;
  GLint fogEnd;
};

struct LineUniforms {
  GLint view;
  GLint projection;
};

struct LanternUniforms {
  GLint view;
  GLint projection;
  GLint model;
};

GroundUniforms makeGroundUniforms(GLuint program);
ObjUniforms makeObjUniforms(GLuint program);
PathUniforms makePathUniforms(GLuint program);
LineUniforms makeLineUniforms(GLuint program);
LanternUniforms makeLanternUniforms(GLuint program);
