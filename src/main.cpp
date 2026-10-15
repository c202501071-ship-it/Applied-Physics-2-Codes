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

constexpr int kCircleSegments = 40;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kPointRadius = 0.03f;
constexpr float kRestitution = 0.4f;
constexpr float kGravity = -1.8f;

struct PointMass {
    float posX, posY;
    float velX, velY;
    float mass;
};

constexpr const char* kVertexShaderSource = R"(#version 330 core
layout (location = 0) in vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
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

GLuint createDynamicVao(GLuint& vbo) {
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return vao;
}

void uploadDynamic(GLuint vbo, const std::vector<float>& data) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void applyGravity(PointMass& point, float deltaTime) {
    point.velY += kGravity * deltaTime;
}

void updatePoint(PointMass& point, float deltaTime) {
    point.posX += point.velX * deltaTime;
    point.posY += point.velY * deltaTime;
}

void resolveWallCollision(PointMass& point) {
    if (point.posX - kPointRadius < -1.0f) {
        point.posX = -1.0f + kPointRadius;
        point.velX = -point.velX;
    }
    if (point.posX + kPointRadius > 1.0f) {
        point.posX = 1.0f - kPointRadius;
        point.velX = -point.velX;
    }
    if (point.posY - kPointRadius < -1.0f) {
        point.posY = -1.0f + kPointRadius;
        point.velY = -point.velY * kRestitution;
    }
    if (point.posY + kPointRadius > 1.0f) {
        point.posY = 1.0f - kPointRadius;
        point.velY = -point.velY;
    }
}

} // namespace

int main() {
    try {
        gfx::GlfwContext glfw;
        gfx::Window window({.width = 600, .height = 600, .title = "Billiards Sim"});

        GLuint program = createProgram(kVertexShaderSource, kFragmentShaderSource);
        vector<float> circleVerts = generateCircleVertices(kPointRadius, kCircleSegments);
        GLuint circleVbo = 0;
        GLuint circleVao = createDynamicVao(circleVbo);
        GLsizei circleVertexCount = static_cast<GLsizei>(circleVerts.size() / 2);

        vector<PointMass> points = {
            {0.0f, 0.6f, 0.0f, 0.0f, 1.0f},
        };

        glClearColor(0.0f, 0.396f, 0.549f, 1.0f);

        float lastFrameTime = static_cast<float>(glfwGetTime());

        while (!window.shouldClose()) {
            float currentFrameTime = static_cast<float>(glfwGetTime());
            float deltaTime = currentFrameTime - lastFrameTime;
            lastFrameTime = currentFrameTime;
            gfx::processInput(window);

            for (PointMass& point : points) {
                applyGravity(point, deltaTime);
                updatePoint(point, deltaTime);
                resolveWallCollision(point);
            }

            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(program);
            glBindVertexArray(circleVao);
            for (const PointMass& point : points) {
                std::vector<float> translated = circleVerts;
                for (size_t i = 0; i < translated.size(); i += 2) {
                    translated[i] += point.posX;
                    translated[i + 1] += point.posY;
                }
                uploadDynamic(circleVbo, translated);
                glDrawArrays(GL_TRIANGLE_FAN, 0, circleVertexCount);
            }

            window.swapBuffers();
            window.pollEvents();
        }

        glDeleteVertexArrays(1, &circleVao);
        glDeleteBuffers(1, &circleVbo);
        glDeleteProgram(program);
    } catch (const exception& e) {
        cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}