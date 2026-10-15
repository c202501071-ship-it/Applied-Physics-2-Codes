#include "core/window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdexcept>

namespace {

void onFramebufferResize(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
}

}  // namespace

namespace gfx {

void Window::Deleter::operator()(GLFWwindow* window) const {
    glfwDestroyWindow(window);
}

Window::Window(const WindowConfig& config) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, config.glMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, config.glMinor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    handle_.reset(glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr));
    if (!handle_) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(handle_.get());

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        throw std::runtime_error("Failed to initialize GLAD");
    }

    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(handle_.get(), onFramebufferResize);

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(handle_.get(), &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(handle_.get()) == GLFW_TRUE;
}

void Window::close() {
    glfwSetWindowShouldClose(handle_.get(), GLFW_TRUE);
}

void Window::swapBuffers() {
    glfwSwapBuffers(handle_.get());
}

void Window::pollEvents() {
    glfwPollEvents();
}

bool Window::isKeyPressed(int key) const {
    return glfwGetKey(handle_.get(), key) == GLFW_PRESS;
}

bool Window::isMouseButtonPressed(int button) const {
    return glfwGetMouseButton(handle_.get(), button) == GLFW_PRESS;
}

void Window::getCursorPosition(double& x, double& y) const {
    glfwGetCursorPos(handle_.get(), &x, &y);
}

}  // namespace gfx
