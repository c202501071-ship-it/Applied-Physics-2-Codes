#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/glfw_context.h"
#include "core/input.h"
#include "core/window.h"

using namespace std;

namespace {

constexpr float kBallRadius = 0.1f;
constexpr int kCircleSegments = 40;
constexpr float kPi = 3.14159265358979323846f;

struct Ball {
    float posX, posY;
    float velX, velY;
    float radius;
};

constexpr const char* kVertexShaderSource = R"(#version 330 core
layout (location = 0) in vec2 aPos;
uniform vec2 uCenter;
void main() { gl_Position = vec4(aPos + uCenter, 0.0, 1.0); }
)";

constexpr const char* kFragmentShaderSource = R"(#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(0.85, 0.85, 0.9, 1.0); }
)";

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        throw runtime_error(string("Shader compilation failed: ") + log);
    }
    return shader;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        glDeleteProgram(program);
        throw runtime_error(string("Program linking failed: ") + log);
    }
    return program;
}

vector<float> generateCircleVertices(float radius, int segments) {
    vector<float> vertices;
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);

    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i/segments * 2.0f * kPi;
        vertices.push_back(radius * cosf(angle));
        vertices.push_back(radius * sinf(angle));
    }
    return vertices;
}

struct CircleMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei vertexCount = 0;
};

CircleMesh createCircleMesh(const vector<float>& vertices) {
    CircleMesh mesh;
    mesh.vertexCount = static_cast<GLsizei>(vertices.size() / 2);

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return mesh;
}

void drawBall(GLuint program, const CircleMesh& mesh, const Ball& ball) {
    glUseProgram(program);
    glUniform2f(glGetUniformLocation(program, "uCenter"), ball.posX, ball.posY);
    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, mesh.vertexCount);
}

void updateBall(Ball& ball, float deltaTime) {
    ball.posX += ball.velX * deltaTime;
    ball.posY += ball.velY * deltaTime;
}

void resolveWallCollision(Ball& ball) {
    if (ball.posX - ball.radius < -1.0f) {
        ball.posX = -1.0f + ball.radius;
        ball.velX = -ball.velX;
    }
    if (ball.posX + ball.radius > 1.0f) {
        ball.posX = 1.0f - ball.radius;
        ball.velX = -ball.velX;
    }
    if (ball.posY - ball.radius < -1.0f) {
        ball.posY = -1.0f + ball.radius;
        ball.velY = -ball.velY;
    }
    if (ball.posY + ball.radius > 1.0f) {
        ball.posY = 1.0f - ball.radius;
        ball.velY = -ball.velY;
    }
}

} // namespace

int main() {
    try {
        gfx::GlfwContext glfw;
        gfx::Window window({.width = 600, .height = 600, .title = "Billiards Sim"});

        GLuint program = createProgram(kVertexShaderSource, kFragmentShaderSource);
        CircleMesh mesh = createCircleMesh(generateCircleVertices(kBallRadius, kCircleSegments));

        int numBalls = 1;
        vector<Ball> balls;
        balls.reserve(numBalls);

        for (size_t i = 0; i < numBalls; ++i) {
            balls.push_back({-0.5f, 0.3f, 0.6f, 0.4f, kBallRadius});
        }

        glClearColor(0.0f, 0.396f, 0.549f, 1.0f);

        float lastFrameTime = static_cast<float>(glfwGetTime());

        while (!window.shouldClose()) {
            float currentFrameTime = static_cast<float>(glfwGetTime());
            float deltaTime = currentFrameTime - lastFrameTime;
            lastFrameTime = currentFrameTime;
            gfx::processInput(window);

            for (Ball& ball : balls) {
                updateBall(ball, deltaTime);
                resolveWallCollision(ball);
            }

            glClear(GL_COLOR_BUFFER_BIT);
            for (const Ball& ball : balls) {
                drawBall(program, mesh, ball);
            }

            window.swapBuffers();
            window.pollEvents();
        }

        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vbo);
        glDeleteProgram(program);
    } catch (const exception& e) {
        cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}