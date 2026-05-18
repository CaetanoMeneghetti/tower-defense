# Este arquivo CMakeLists.txt foi adaptado a partir do projeto castor
# do PET INF/UFRGS (https://github.com/petcomputacaoufrgs/castor-fcg),
# com algumas modificações vindas do arquivo CMakeLists.txt criado
# pelos alunos Luis Melo e Santiago Gonzaga em 2023/1.

# Arquivos fonte C/C++. Inclua nesta lista todos os arquivos que devem
# ser compilados.
set(SOURCES
  src/main.cpp

  # engine — primitivas reutilizáveis (modelos, mesh, shaders, câmera, HUD…)
  src/engine/animated_model.cpp
  src/engine/camera.cpp
  src/engine/catmull_rom.cpp
  src/engine/game_object.cpp
  src/engine/hud.cpp
  src/engine/lighting.cpp
  src/engine/mesh.cpp
  src/engine/obj_loader.cpp
  src/engine/primitive_meshes.cpp
  src/engine/shader_program.cpp
  src/engine/texture_loader.cpp

  # game — regras de jogo (combate, placement, geração de cena)
  src/game/defender_system.cpp
  src/game/enemy_system.cpp
  src/game/path_navigation.cpp
  src/game/scene.cpp
  src/game/troop_placement.cpp
  src/game/troop_selection.cpp
  src/game/upgrade_system.cpp

  # input — callbacks GLFW + controle de câmera
  src/input/camera_controller.cpp
  src/input/input_handler.cpp

  # render — desenho da cena
  src/render/scene_renderer.cpp
  src/render/shader_uniforms.cpp

  # world — geração procedural do caminho de terra
  src/world/path_generator.cpp

  # utils / 3rd-party
  src/utils/glad.c
  src/imgui/imgui.cpp
  src/imgui/imgui_draw.cpp
  src/imgui/imgui_tables.cpp
  src/imgui/imgui_widgets.cpp
  src/imgui/imgui_impl_glfw.cpp
  src/imgui/imgui_impl_opengl3.cpp
)

cmake_minimum_required(VERSION 3.5.0)

project(LAB_FCG VERSION 1.0.0)

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# Garante que <GLFW/glfw3.h> nao puxe gl.h (evita conflito com glad)
add_definitions(-DGLFW_INCLUDE_NONE)

if(WIN32)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${PROJECT_SOURCE_DIR}/bin/Debug")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/bin/Release")
elseif(UNIX)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/Linux")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
message(STATUS
  "Build type: ${CMAKE_BUILD_TYPE}

               Change the build type on the command line with

                   -DCMAKE_BUILD_TYPE=type

               for type in {Release, Debug, RelWithDebInfo}.
")

set(EXECUTABLE_NAME main)

# Verifica se todos os arquivos fonte estão presentes no diretório
# atual. Se não estão, avisa sobre CMakeLists mal configurado.
foreach(source_file IN LISTS SOURCES)
  if(NOT EXISTS ${PROJECT_SOURCE_DIR}/${source_file})
    message(FATAL_ERROR "
O arquivo ${PROJECT_SOURCE_DIR}/${source_file} não existe.
Por favor, atualize a lista de arquivos fonte no arquivo CMakeLists.txt.")
    break()
  endif()
endforeach()

include(FetchContent)
#remoção de módulos inuteis pra compilar + rapido
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Força biblioteca estática para fugir do erro do windres" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "testes do Assimp" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "ferramentas de linha de comando" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "Compilar o zlib internamente" FORCE)
set(INJECT_DEBUG_POSTFIX OFF CACHE BOOL "Evita renomear a lib no modo debug" FORCE)

FetchContent_declare(
  assimp 
  GIT_REPOSITORY https://github.com/assimp/assimp.git
  GIT_TAG v5.4.1
)


if(MINGW OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  # Injeta o big-obj diretamente nas variáveis globais do C++ para o Assimp não ignorar
  string(APPEND CMAKE_CXX_FLAGS " -Wa,-mbig-obj")
  string(APPEND CMAKE_C_FLAGS " -Wa,-mbig-obj")
endif()

FetchContent_MakeAvailable(assimp)

add_executable(${EXECUTABLE_NAME} ${SOURCES})

add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${PROJECT_SOURCE_DIR}/data
          $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>/data
)

target_include_directories(${EXECUTABLE_NAME} BEFORE PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/imgui
)

if(WIN32)

  if(MINGW)

    # Aqui tentamos descobrir qual libc do Widows está sendo usada
    # pelo compilador MinGW: msvcrt (antiga) ou ucrt (nova). Também
    # diferenciamos entre um compilador 32-bits (antigo) ou 64-bits.
    # Para isso, buscamos pela ocorrência de algumas strings
    # específicas no output do comando "-v" do GCC, que lista os
    # parâmetros de configuração do compilador.
    # TODO: Testar com compilador llvm/clang.
    execute_process(
      COMMAND ${CMAKE_CXX_COMPILER} "-v"
      ERROR_VARIABLE  COMPILER_VERSION_OUTPUT
      RESULT_VARIABLE COMPILER_VERSION_RESULT
    )

    if (COMPILER_VERSION_RESULT EQUAL 0)
      # NOTE: É importante que o primeiro teste seja buscando pela
      # string ucrt64 no output do compilador, pois a string "mingw64"
      # sempre aparece no output (mesmo quando ucrt64 é a libc utilizada).
      if (COMPILER_VERSION_OUTPUT MATCHES "ucrt64")
        set(LIBGLFW ${PROJECT_SOURCE_DIR}/lib-ucrt-64/libglfw3.a)
      elseif (COMPILER_VERSION_OUTPUT MATCHES "mingw64")
        set(LIBGLFW ${PROJECT_SOURCE_DIR}/lib-mingw-64/libglfw3.a)
      else()
        set(LIBGLFW ${PROJECT_SOURCE_DIR}/lib-mingw-32/libglfw3.a)
      endif()
    else()
      message(FATAL_ERROR "Failed to get MinGW compiler version.")
    endif()

  elseif(MSVC)
    set(LIBGLFW ${PROJECT_SOURCE_DIR}/lib-vc2022/glfw3.lib)
  else()
    message(FATAL_ERROR "This CMakeLists.txt file only supports MINGW or MSVC toolchain on Windows.")
  endif()

  message(STATUS "LIBGLFW = ${LIBGLFW}")

  target_link_libraries(${EXECUTABLE_NAME} ${LIBGLFW} gdi32 opengl32 assimp::assimp)

elseif(UNIX)

  target_compile_options(${EXECUTABLE_NAME} PRIVATE -Wall -Wno-unused-function)

  # Add custom target for 'run'
  add_custom_target(run
      COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ./main
      DEPENDS main
      USES_TERMINAL
  )

  find_package(OpenGL REQUIRED)
  find_package(X11 REQUIRED)
  find_library(MATH_LIBRARY m)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  target_link_libraries(${EXECUTABLE_NAME}
    ${CMAKE_DL_LIBS}
    ${MATH_LIBRARY}
    ${PROJECT_SOURCE_DIR}/lib-linux/libglfw3.a
    ${CMAKE_THREAD_LIBS_INIT}
    ${OPENGL_LIBRARIES}
    ${X11_LIBRARIES}
    ${X11_Xrandr_LIB}
    ${X11_Xcursor_LIB}
    ${X11_Xinerama_LIB}
    assimp::assimp
  )

endif()