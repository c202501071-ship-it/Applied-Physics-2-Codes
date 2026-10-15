cmake_minimum_required(VERSION 4.3)
project(OpenGLTest)
cmake_policy(SET CMP0072 NEW)

find_package(OpenGL REQUIRED)
set(CMAKE_CXX_STANDARD 20)

add_library(glad STATIC ../../opengl-cpp-setup/src/glad.c)

file(GLOB EXPERIMENT_FILES
        "src/*.cpp"
        "assessments/*/*.cpp"
)

foreach(FILE_PATH ${EXPERIMENT_FILES})
    get_filename_component(EXE_NAME ${FILE_PATH} NAME_WE)

    add_executable(${EXE_NAME}
            ${FILE_PATH}
            "../../opengl-cpp-setup/src/core/glfw_context.cpp"
            "../../opengl-cpp-setup/src/core/input.cpp"
            "../../opengl-cpp-setup/src/core/window.cpp"
    )
    target_include_directories(${EXE_NAME} PRIVATE include src)
    # Link your executable to glfw, OpenGL, and our compiled glad library
    target_link_libraries(${EXE_NAME} glfw OpenGL::GL glad)
endforeach()