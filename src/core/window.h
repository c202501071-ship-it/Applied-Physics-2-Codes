#pragma once

#include <memory>
#include <string>

struct GLFWwindow;

namespace gfx {

struct WindowConfig {
    int width = 800;
    int height = 600;
    std::string title = "OpenGL";
    int glMajor = 3;
    int glMinor = 3;
};

class Window {
public:
    explicit Window(const WindowConfig& config);

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void close();
    void swapBuffers();
    void pollEvents();
    bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;
    void getCursorPosition(double& x, double& y) const;

private:
    struct Deleter {
        void operator()(GLFWwindow* window) const;
    };

    std::unique_ptr<GLFWwindow, Deleter> handle_;
};

}  // namespace gfx
