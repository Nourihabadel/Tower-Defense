
cmake_minimum_required(VERSION 3.10)
project(OpenGLStarter)

add_subdirectory(external/glfw-3.5.1)
add_subdirectory(external/glm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

add_executable(OpenGLStarter
    main.cpp
)

target_include_directories(OpenGLStarter PRIVATE external/glad/include)
target_link_libraries(OpenGLStarter PRIVATE glfw OpenGL::GL glm::glm)