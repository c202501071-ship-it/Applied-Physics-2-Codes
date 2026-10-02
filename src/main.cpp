#include <glad/glad.h>

#include <cstdlib>
#include <exception>
#include <iostream>

#include "core/glfw_context.h"
#include "core/input.h"
#include "core/window.h"

int main() {
    try {
        gfx::GlfwContext glfw;
        gfx::Window window({.width = 800, .height = 600, .title = "Villacastin OpenGL"});

        glClearColor(0.54f, 0.7f, 0.32f, 1.0f);

        while (!window.shouldClose()) {
            gfx::processInput(window);

            glClear(GL_COLOR_BUFFER_BIT);

            window.swapBuffers();
            window.pollEvents();
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
