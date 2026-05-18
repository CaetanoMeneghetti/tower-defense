#include "render/shader_uniforms.h"

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
  };
}

LanternUniforms makeLanternUniforms(GLuint program) {
  return LanternUniforms{
      glGetUniformLocation(program, "view"),
      glGetUniformLocation(program, "projection"),
      glGetUniformLocation(program, "model"),
  };
}
