#pragma once

#include <string>

unsigned int compileShaderFromFile(unsigned int shaderType, const std::string &path);
unsigned int createShaderProgram(const std::string &vertexPath,
                                 const std::string &fragmentPath);
