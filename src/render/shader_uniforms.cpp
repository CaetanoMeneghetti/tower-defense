#include "render/shader_uniforms.h"

#include <string>
#include <unordered_map>

GLint cachedUniformLocation(GLuint program, const char *name) {
  // (program, nome) -> location. Preenchido sob demanda no 1º acesso.
  static std::unordered_map<GLuint, std::unordered_map<std::string, GLint>> cache;
  auto &progCache = cache[program];
  auto it = progCache.find(name);
  if (it != progCache.end()) return it->second;
  GLint loc = glGetUniformLocation(program, name);
  progCache.emplace(name, loc);
  return loc;
}

GroundUniforms makeGroundUniforms(GLuint program) {
  return GroundUniforms{
      glGetUniformLocation(program, "view"),
      glGetUniformLocation(program, "projection"),
      glGetUniformLocation(program, "grass"),
      glGetUniformLocation(program, "noise"),
      glGetUniformLocation(program, "fogColor"),
      glGetUniformLocation(program, "fogStart"),
      glGetUniformLocation(program, "fogEnd"),
  };
}

ObjUniforms makeObjUniforms(GLuint program) {
  return ObjUniforms{
      glGetUniformLocation(program, "view"),
      glGetUniformLocation(program, "projection"),
      glGetUniformLocation(program, "model"),
  };
}

PathUniforms makePathUniforms(GLuint program) {
  return PathUniforms{
      glGetUniformLocation(program, "view"),
      glGetUniformLocation(program, "projection"),
      glGetUniformLocation(program, "model"),
      glGetUniformLocation(program, "dirt"),
      glGetUniformLocation(program, "noise"),
      glGetUniformLocation(program, "fogColor"),
      glGetUniformLocation(program, "fogStart"),
      glGetUniformLocation(program, "fogEnd"),
  };
}

LineUniforms makeLineUniforms(GLuint program) {
  return LineUniforms{
      glGetUniformLocation(program, "view"),
      glGetUniformLocation(program, "projection"),
      glGetUniformLocation(program, "model"),
      glGetUniformLocation(program, "lineColor"),
  };
}

LanternUniforms makeLanternUniforms(GLuint program) {
  return LanternUniforms{
      glGetUniformLocation(program, "view"),
      glGetUniformLocation(program, "projection"),
      glGetUniformLocation(program, "model"),
  };
}
