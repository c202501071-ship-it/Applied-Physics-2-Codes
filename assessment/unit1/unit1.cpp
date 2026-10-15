// NAME IS unit.cpp BECAUSE IDE REFUSES TO RUN THIS CODE IF IT IS NAMED main.cpp

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
constexpr float kDt = 1.0f / 60.0f;

// --- Assessment Constants ---
constexpr float kTurretRadius = 0.07f;
constexpr float kBallRadius = 0.04f;
constexpr float kAimLineLength = 0.25f;
constexpr float kRotationSpeed = 1.5f;       // Radians per second
constexpr float kMinAngle = 20.0f * kPi / 180.0f;  // 20 degrees in rad
constexpr float kMaxAngle = 160.0f * kPi / 180.0f; // 160 degrees in rad
constexpr float kBallSpeed = 1.6f;

// --- State Variables ---
float g_Angle = kPi / 2.0f;                  // Starts pointing straight up (90 degrees)
float g_BallX = 0.0f;
float g_BallY = -0.85f;
float g_VelX = 0.0f;
float g_VelY = 0.0f;
bool g_IsFired = false;
bool g_SpaceWasPressed = false;              // To detect clean single key press transitions

// --- Shader Sources ---
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
        float angle = (float)i / segments * 2.0f * kPi;
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

// Generates VBO data offsets for drawing circles cleanly centered anywhere
vector<float> getTranslatedCircle(const vector<float>& baseCircle, float cx, float cy) {
    vector<float> translated = baseCircle;
    for (size_t i = 0; i < translated.size(); i += 2) {
        translated[i] += cx;
        translated[i + 1] += cy;
    }
    return translated;
}

} // namespace

int main() {
    try {
        gfx::GlfwContext glfw;
        gfx::Window window({.width = 600, .height = 600, .title = "Unit I - Villacastin"});

        GLuint program = createProgram(kVertexShaderSource, kFragmentShaderSource);
        GLint colorLoc = glGetUniformLocation(program, "uColor");

        // Prepare Shape Vertex Buffers
        vector<float> turretCircleVerts = generateCircleVertices(kTurretRadius, kCircleSegments);
        vector<float> ballCircleVerts = generateCircleVertices(kBallRadius, kCircleSegments);

        GLuint shapeVbo = 0;
        GLuint shapeVao = createDynamicVao(shapeVbo);
        GLsizei turretCount = static_cast<GLsizei>(turretCircleVerts.size() / 2);
        GLsizei ballCount = static_cast<GLsizei>(ballCircleVerts.size() / 2);

        GLuint lineVbo = 0;
        GLuint lineVao = createDynamicVao(lineVbo);

        // Step 1: Clear canvas to assigned Blue Background color (0.08, 0.28, 0.42)
        glClearColor(0.08f, 0.28f, 0.42f, 1.0f);

        while (!window.shouldClose()) {
            gfx::processInput(window);

            // --- Step 3: Handle Aim Input and Rotation Limits ---
            if (window.isKeyPressed(GLFW_KEY_LEFT)) {
                g_Angle += kRotationSpeed * kDt;
                if (g_Angle > kMaxAngle) g_Angle = kMaxAngle;
            }
            if (window.isKeyPressed(GLFW_KEY_RIGHT)) {
                g_Angle -= kRotationSpeed * kDt;
                if (g_Angle < kMinAngle) g_Angle = kMinAngle;
            }

            // --- Step 4: Edge-Triggered Spacebar Firing Trigger ---
            bool spaceIsDown = window.isKeyPressed(GLFW_KEY_SPACE);
            if (spaceIsDown && !g_SpaceWasPressed) {
                g_BallX = 0.0f;
                g_BallY = -0.85f;
                g_VelX = kBallSpeed * cosf(g_Angle);
                g_VelY = kBallSpeed * sinf(g_Angle);
                g_IsFired = true;
            }
            g_SpaceWasPressed = spaceIsDown;

            // --- Step 4: Euler Integration Physics & Wall Bounce Handling ---
            if (g_IsFired) {
                g_BallX += g_VelX * kDt;
                g_BallY += g_VelY * kDt;

                // Right Wall Bounce
                if (g_BallX + kBallRadius > 1.0f) {
                    g_VelX = -g_VelX;
                    g_BallX = 1.0f - kBallRadius;
                }
                // Left Wall Bounce
                if (g_BallX - kBallRadius < -1.0f) {
                    g_VelX = -g_VelX;
                    g_BallX = -1.0f + kBallRadius;
                }
                // Top Wall Bounce
                if (g_BallY + kBallRadius > 1.0f) {
                    g_VelY = -g_VelY;
                    g_BallY = 1.0f - kBallRadius;
                }
                // Bottom Wall Bounce
                if (g_BallY - kBallRadius < -1.0f) {
                    g_VelY = -g_VelY;
                    g_BallY = -1.0f + kBallRadius;
                }
            } else {
                // If not fired yet, white ball stays resting securely on top of the turret base
                g_BallX = 0.0f;
                g_BallY = -0.85f;
            }

            // --- Render Logic ---
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(program);

            // 1. Draw White Aim Line (0.25 Units Long extending outward from the Turret Core)
            float lineEndX = 0.0f + kAimLineLength * cosf(g_Angle);
            float lineEndY = -0.85f + kAimLineLength * sinf(g_Angle);
            vector<float> lineVerts = { 0.0f, -0.85f, lineEndX, lineEndY };

            uploadDynamic(lineVbo, lineVerts);
            glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f); // White
            glBindVertexArray(lineVao);
            glDrawArrays(GL_LINES, 0, 2);

            // 2. Draw Gray Turret Base Circle at (0.0, -0.85)
            vector<float> turretRenderVerts = getTranslatedCircle(turretCircleVerts, 0.0f, -0.85f);
            uploadDynamic(shapeVbo, turretRenderVerts);
            glUniform4f(colorLoc, 0.5f, 0.5f, 0.5f, 1.0f); // Gray
            glBindVertexArray(shapeVao);
            glDrawArrays(GL_TRIANGLE_FAN, 0, turretCount);

            // 3. Draw White Shot Ball Circle
            vector<float> ballRenderVerts = getTranslatedCircle(ballCircleVerts, g_BallX, g_BallY);
            uploadDynamic(shapeVbo, ballRenderVerts);
            glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f); // White
            glBindVertexArray(shapeVao);
            glDrawArrays(GL_TRIANGLE_FAN, 0, ballCount);

            window.swapBuffers();
            window.pollEvents();
        }

        glDeleteVertexArrays(1, &shapeVao);
        glDeleteBuffers(1, &shapeVbo);
        glDeleteVertexArrays(1, &lineVao);
        glDeleteBuffers(1, &lineVbo);
        glDeleteProgram(program);
    } catch (const exception& e) {
        cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
