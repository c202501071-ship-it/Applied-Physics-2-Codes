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
constexpr float kGravity = -1.8f;
constexpr float kDt = 1.0f / 60.0f;

struct Point {
    float posX, posY, prevX, prevY;
    bool pinned = false;
};

struct Stick {
    int a, b;
    float restLength;
};

struct Vec2 {
    float x, y;
};

constexpr const char* kVertexShaderSource = R"(#version 330 core
layout (location = 0) in vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
)";

constexpr const char* kFragmentShaderSource = R"(#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() { FragColor = uColor; }
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

void uploadDynamic(GLuint vbo, const vector<float>& data) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

float distance(const Point& a, const Point& b) {
    float dx = b.posX - a.posX;
    float dy = b.posY - a.posY;
    return sqrtf(dx * dx + dy * dy);
}

void updatePoint(Point& point, float dt) {
    if (point.pinned) return;
    float oldX = point.posX, oldY = point.posY;
    point.posX += point.posX - point.prevX;
    point.posY += point.posY - point.prevY + kGravity * dt * dt;
    point.prevX = oldX;
    point.prevY = oldY;
}

void applyStick(std::vector<Point>& points, const Stick& stick) {
    Point& a = points[stick.a];
    Point& b = points[stick.b];
    float dist = distance(a, b);
    if (dist < 1e-6f) return;
    float error = dist - stick.restLength;
    Vec2 dir = {(b.posX - a.posX) / dist, (b.posY - a.posY) / dist};
    float wA = a.pinned ? 0.0f : (b.pinned ? 1.0f : 0.5f);
    float wB = b.pinned ? 0.0f : (a.pinned ? 1.0f : 0.5f);
    a.posX += dir.x * error * wA;
    a.posY += dir.y * error * wA;
    b.posX -= dir.x * error * wB;
    b.posY -= dir.y * error * wB;
}

} // namespace

int main() {
    try {
        gfx::GlfwContext glfw;
        gfx::Window window({.width = 600, .height = 600, .title = "Billiards Sim"});

        GLuint program = createProgram(kVertexShaderSource, kFragmentShaderSource);
        GLint colorLoc = glGetUniformLocation(program, "uColor");
        vector<float> circleVerts = generateCircleVertices(kPointRadius, kCircleSegments);
        GLuint circleVbo = 0;
        GLuint circleVao = createDynamicVao(circleVbo);
        GLsizei circleVertexCount = static_cast<GLsizei>(circleVerts.size() / 2);
        GLuint lineVbo = 0;
        GLuint lineVao = createDynamicVao(lineVbo);

        vector<Point> points = {
            {-0.35f, 0.8f, -0.35f, 0.8f},
            {-0.25f, 0.8f, -0.25f, 0.8f},
            {-0.15f, 0.8f, -0.15f, 0.8f},
            {-0.05f, 0.8f, -0.05f, 0.8f},
            {0.05f, 0.8f, 0.05f, 0.8f},
            {0.15f, 0.8f, 0.15f, 0.8f},
            {0.25f, 0.8f, 0.25f, 0.8f},
            {0.35f, 0.8f, 0.35f, 0.8f},
        };

        std::vector<Stick> sticks = {
            {0, 1, distance(points[0], points[1])},
            {1, 2, distance(points[1], points[2])},
            {2, 3, distance(points[2], points[3])},
            {3, 4, distance(points[3], points[4])},
            {4, 5, distance(points[4], points[5])},
            {5, 6, distance(points[5], points[6])},
            {6, 7, distance(points[6], points[7])},
        };

        points[0].pinned = true;

        glClearColor(0.0f, 0.396f, 0.549f, 1.0f);

        while (!window.shouldClose()) {
            gfx::processInput(window);

            for (Point& point : points) {
                updatePoint(point, kDt);
            }

            for (const Stick& stick : sticks) {
                applyStick(points, stick);
            }

            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(program);

            vector<float> lineVerts;
            for (const Stick& stick : sticks) {
                lineVerts.push_back(points[stick.a].posX);
                lineVerts.push_back(points[stick.a].posY);
                lineVerts.push_back(points[stick.b].posX);
                lineVerts.push_back(points[stick.b].posY);
            }
            uploadDynamic(lineVbo, lineVerts);
            glUniform4f(colorLoc, 0.31f, 0.76f, 0.97f, 1.0f);
            glBindVertexArray(lineVao);
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVerts.size() / 2));

            glUniform4f(colorLoc, 0.85f, 0.85f, 0.9f, 1.0f);
            glBindVertexArray(circleVao);
            for (const Point& point : points) {
                vector<float> translated = circleVerts;
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
        glDeleteVertexArrays(1, &lineVao);
        glDeleteBuffers(1, &lineVbo);
        glDeleteProgram(program);
    } catch (const exception& e) {
        cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}