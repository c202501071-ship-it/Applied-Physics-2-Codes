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

struct Vec2 {
    float x, y;
};

struct Ball {
    float x, y;
    float velX, velY;
    float radius;
    float restitution;
    float r, g, b;
};

namespace {

constexpr int kCircleSegments = 40;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDt = 1.0f / 60.0f;

// --- Assessment Constants ---
constexpr float kTurretRadius = 0.07f;
constexpr float kAimLineLength = 0.25f;
constexpr float kRotationSpeed = 1.5f;             // Radians per second
constexpr float kMinAngle = 20.0f * kPi / 180.0f;  // 20 degrees in rad
constexpr float kMaxAngle = 160.0f * kPi / 180.0f; // 160 degrees in rad
constexpr float kBallSpeed = 1.6f;

// --- State Variables ---
float g_Angle = kPi / 2.0f;                        // Starts pointing straight up (90 degrees)
bool g_IsFired = false;
bool g_SpaceWasPressed = false;                    // To detect clean single key press transitions

// --- Global Balls Vector (Updated per specification) ---
vector<Ball> g_Balls = {
    // x,      y,       velX,  velY,  radius, restitution, r,    g,    b
    {0.0f,  -0.85f,     0.0f,  0.0f,   0.04f,     0.9f,    1.0f, 1.0f, 1.0f}, // Shot (White)
    {-0.45f, 0.45f,     0.0f,  0.0f,   0.10f,     0.6f,    1.0f, 0.0f, 0.0f}, // Red
    {0.0f,   0.60f,     0.0f,  0.0f,   0.07f,     0.9f,    1.0f, 1.0f, 0.0f}, // Yellow
    {0.45f,  0.35f,     0.0f,  0.0f,   0.05f,     0.3f,    0.0f, 0.0f, 1.0f}  // Blue
};

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

// Generates a base circle with unit radius (1.0)
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

// Scaled and translates a unit circle dynamically
vector<float> getTranslatedCircle(const vector<float>& baseCircle, float cx, float cy, float r) {
    vector<float> translated = baseCircle;
    for (size_t i = 0; i < translated.size(); i += 2) {
        translated[i] = baseCircle[i] * r + cx;
        translated[i + 1] = baseCircle[i + 1] * r + cy;
    }
    return translated;
}

bool checkBallCollision(const Ball& a, const Ball& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float distance = sqrt(dx * dx + dy * dy);
    return distance < (a.radius + b.radius);
}

Vec2 computeCollisionNormal(const Ball& a, const Ball& b, float& outDistance) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    outDistance = sqrt(dx * dx + dy * dy);
    return { dx / outDistance, dy / outDistance };
}

void exchangeNormalVelocity(Ball& a, Ball& b, const Vec2& normal) {
    float dotA = a.velX * normal.x + a.velY * normal.y;
    float dotB = b.velX * normal.x + b.velY * normal.y;

    float impulse = dotB - dotA;

    a.velX += impulse * normal.x;
    a.velY += impulse * normal.y;
    b.velX -= impulse * normal.x;
    b.velY -= impulse * normal.y;
}

void correctPenetration(Ball& a, Ball& b, const Vec2& normal, float distance) {
    float overlap = (a.radius + b.radius) - distance;
    float pushEach = overlap / 2.0f;

    a.x -= normal.x * pushEach;
    a.y -= normal.y * pushEach;
    b.x += normal.x * pushEach; // Fixed typo (was modifying b.y twice)
    b.y += normal.y * pushEach;
}

void resolveBallCollision(Ball& a, Ball& b) {
    float distance;
    Vec2 normal = computeCollisionNormal(a, b, distance);

    exchangeNormalVelocity(a, b, normal);
    correctPenetration(a, b, normal, distance);
}

} // namespace

int main() {
    try {
        gfx::GlfwContext glfw;
        gfx::Window window({.width = 600, .height = 600, .title = "Midterm Part 1 - Villacastin"});

        GLuint program = createProgram(kVertexShaderSource, kFragmentShaderSource);
        GLint colorLoc = glGetUniformLocation(program, "uColor");

        // Generate base structures using generic sizes
        vector<float> baseCircleVerts = generateCircleVertices(1.0f, kCircleSegments);

        GLuint shapeVbo = 0;
        GLuint shapeVao = createDynamicVao(shapeVbo);
        GLsizei circleCount = static_cast<GLsizei>(baseCircleVerts.size() / 2);

        GLuint lineVbo = 0;
        GLuint lineVao = createDynamicVao(lineVbo);

        // Step 1: Clear canvas to assigned Blue Background color
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
                g_Balls[0].x = 0.0f;
                g_Balls[0].y = -0.85f;
                g_Balls[0].velX = kBallSpeed * cosf(g_Angle);
                g_Balls[0].velY = kBallSpeed * sinf(g_Angle);
                g_IsFired = true;
            }
            g_SpaceWasPressed = spaceIsDown;

            // --- Step 4: Euler Integration Physics & Wall Bounce Handling ---
            for (size_t i = 0; i < g_Balls.size(); ++i) {
                if (i == 0 && !g_IsFired) {
                    // White ball rests securely on top of the turret base when not fired
                    g_Balls[0].x = 0.0f;
                    g_Balls[0].y = -0.85f;
                    continue;
                }

                // Physics movement step
                g_Balls[i].x += g_Balls[i].velX * kDt;
                g_Balls[i].y += g_Balls[i].velY * kDt;

                // Right Wall Bounce
                if (g_Balls[i].x + g_Balls[i].radius > 1.0f) {
                    g_Balls[i].velX = -g_Balls[i].velX * g_Balls[i].restitution;
                    g_Balls[i].x = 1.0f - g_Balls[i].radius;
                }
                // Left Wall Bounce
                if (g_Balls[i].x - g_Balls[i].radius < -1.0f) {
                    g_Balls[i].velX = -g_Balls[i].velX * g_Balls[i].restitution;
                    g_Balls[i].x = -1.0f + g_Balls[i].radius;
                }
                // Top Wall Bounce
                if (g_Balls[i].y + g_Balls[i].radius > 1.0f) {
                    g_Balls[i].velY = -g_Balls[i].velY * g_Balls[i].restitution;
                    g_Balls[i].y = 1.0f - g_Balls[i].radius;
                }
                // Bottom Wall Bounce
                if (g_Balls[i].y - g_Balls[i].radius < -1.0f) {
                    g_Balls[i].velY = -g_Balls[i].velY * g_Balls[i].restitution;
                    g_Balls[i].y = -1.0f + g_Balls[i].radius;
                }
            }

            // --- Multi-Ball Collision Check Processing ---
            for (size_t i = 0; i < g_Balls.size(); ++i) {
                for (size_t j = i + 1; j < g_Balls.size(); ++j) {
                    if (checkBallCollision(g_Balls[i], g_Balls[j])) {
                        resolveBallCollision(g_Balls[i], g_Balls[j]);
                    }
                }
            }
            // --- Render Logic ---
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(program);
            // 1. Draw White Aim Line
            float lineEndX = 0.0f + kAimLineLength * cosf(g_Angle);
            float lineEndY = -0.85f + kAimLineLength * sinf(g_Angle);
            vector lineVerts = { 0.0f, -0.85f, lineEndX, lineEndY };
            uploadDynamic(lineVbo, lineVerts);
            glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f); // White
            glBindVertexArray(lineVao);
            glDrawArrays(GL_LINES, 0, 2);
            // 2. Draw Gray Turret Base Circle at (0.0, -0.85)
            vector turretRenderVerts = getTranslatedCircle(baseCircleVerts, 0.0f, -0.85f, kTurretRadius);
            uploadDynamic(shapeVbo, turretRenderVerts);
            glUniform4f(colorLoc, 0.5f, 0.5f, 0.5f, 1.0f); // Gray
            glBindVertexArray(shapeVao);
            glDrawArrays(GL_TRIANGLE_FAN, 0, circleCount);
            // 3. Draw All Dynamic Scene Balls
            glBindVertexArray(shapeVao);
            for (const auto& ball : g_Balls) {
                vector ballRenderVerts = getTranslatedCircle(baseCircleVerts, ball.x, ball.y, ball.radius);
                uploadDynamic(shapeVbo, ballRenderVerts);
                glUniform4f(colorLoc, ball.r, ball.g, ball.b, 1.0f);
                glDrawArrays(GL_TRIANGLE_FAN, 0, circleCount);
            }
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