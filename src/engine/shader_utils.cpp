#include "engine/shader_utils.h"

#include <glad/glad.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// Lê um arquivo de texto inteiro e retorna como string
std::string readTextFile(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Nao foi possivel abrir arquivo: " + path);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// Compila um shader a partir de um arquivo
unsigned int compileShaderFromFile(unsigned int shaderType, const std::string &path) {
  // Lê o código do shader do arquivo
  const std::string source = readTextFile(path);
  const char *sourcePtr = source.c_str();

  // Cria o shader no OpenGL
  unsigned int shader = glCreateShader(shaderType);

  // Envia o código fonte para o shader e compila
  glShaderSource(shader, 1, &sourcePtr, NULL);
  glCompileShader(shader);

  // Trata possível erro
  int success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[1024];
    glGetShaderInfoLog(shader, sizeof(infoLog), NULL, infoLog);
    std::cerr << "Erro ao compilar shader " << path << ":\n" << infoLog << "\n";
    glDeleteShader(shader);
    return 0;
  }

  // Retorna shader compilado
  return shader;
}

// Cria um programa de shader (vertex + fragment)
unsigned int createShaderProgram(const std::string &vertexPath,
                                 const std::string &fragmentPath) {
  // Compila vertex e fragment shader
  unsigned int vertexShader = compileShaderFromFile(GL_VERTEX_SHADER, vertexPath);
  unsigned int fragmentShader = compileShaderFromFile(GL_FRAGMENT_SHADER, fragmentPath);

  // Trata possível erro
  if (vertexShader == 0 || fragmentShader == 0) {
    if (vertexShader != 0) {
      glDeleteShader(vertexShader);
    }
    if (fragmentShader != 0) {
      glDeleteShader(fragmentShader);
    }
    return 0;
  }

  // Cria programa de shader e liga os shaders compilados
  unsigned int shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);

  // Trata possível erro
  int success = 0;
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[1024];
    glGetProgramInfoLog(shaderProgram, sizeof(infoLog), NULL, infoLog);
    std::cerr << "Erro ao linkar programa de shader:\n" << infoLog << "\n";
    glDeleteProgram(shaderProgram);
    shaderProgram = 0;
  }

  // Shaders individuais não são mais necessários após link
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  
  // Retorna programa final
  return shaderProgram;
}
