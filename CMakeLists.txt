cmake_minimum_required(VERSION 4.3)
project(OpenGLTest)
cmake_policy(SET CMP0072 NEW)

find_package(OpenGL REQUIRED)
set(CMAKE_CXX_STANDARD 20)

add_library(glad STATIC ../../AppPhy2/glad.c)

file(GLOB EXPERIMENT_FILES "src/*.cpp")

foreach(FILE_PATH ${EXPERIMENT_FILES})
    get_filename_component(EXE_NAME ${FILE_PATH} NAME_WE)

    add_executable(${EXE_NAME}
            ${FILE_PATH}
            "../../AppPhy2/core/glfw_context.cpp"
            "../../AppPhy2/core/input.cpp"
            "../../AppPhy2/core/window.cpp"
    )
    target_include_directories(${EXE_NAME} PRIVATE ../../AppPhy2)
    # Link your executable to glfw, OpenGL, and our compiled glad library
    target_link_libraries(${EXE_NAME} glfw OpenGL::GL glad)
endforeach()