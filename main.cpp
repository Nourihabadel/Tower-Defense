
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>

// Window dimensions
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

// Grid boundaries
const float LEFT_X = -0.9f;
const float RIGHT_X = 0.9f;
const float TOP_Y = 0.7f;
const float BOTTOM_Y = -0.7f;

// Lane boundaries
const float LANE_1_Y = 0.35f;
const float LANE_2_Y = 0.0f;
const float LANE_3_Y = -0.35f;

// Home base boundary
const float BASE_RIGHT_X = -0.65f;

// Selected lane: -1 means no lane selected
int selectedLane = -1;

// ------------------------------------------------------------
// Person 1: Enemy representation and entity collection
// ------------------------------------------------------------
enum class EnemyType {
    Easy,
    Medium,
    Hard
};

struct Enemy {
    EnemyType type;
    int laneIndex;       // 0 to 3
    float x;             // Horizontal OpenGL coordinate
    bool active;         // false when defeated
};

std::vector<Enemy> enemies;

// Wave configuration.
const int MAX_ACTIVE_ENEMIES = 10;
const int TOTAL_WAVES = 3;
const int WAVE_ENEMY_COUNTS[TOTAL_WAVES] = {3, 5, 10};
const double SPAWN_INTERVAL = 2.0;
const float ENEMY_SPEED = 0.12f; // OpenGL units per second, moving toward the base.

int currentWave = 1;
int spawnedInCurrentWave = 0;
double lastSpawnTime = 0.0;
double previousFrameTime = 0.0;
bool gameOver = false;
bool playerWon = false;

int getActiveEnemyCount();
bool areAllEnemiesDefeated();

// Register an enemy in the world.
void addEnemy(EnemyType type, int laneIndex, float x) {
    if (laneIndex < 0 || laneIndex > 3) {
        std::cerr << "Invalid enemy lane: " << laneIndex << std::endl;
        return;
    }

    enemies.push_back({type, laneIndex, x, true});
    std::cout << "Enemy added. Active enemies: "
              << getActiveEnemyCount() << std::endl;
}

// Choose one of the four lanes at random for each new enemy.
int randomLane() {
    return rand() % 4;
}

// Spawn an enemy at the right edge of the grid in a random lane.
void spawnEnemy(EnemyType type) {
    if (getActiveEnemyCount() >= MAX_ACTIVE_ENEMIES) {
        std::cout << "Spawn skipped: active-enemy limit reached ("
                  << MAX_ACTIVE_ENEMIES << ")." << std::endl;
        return;
    }

    const int lane = randomLane();
    addEnemy(type, lane, RIGHT_X);

    std::cout << "Spawned "
              << (type == EnemyType::Easy ? "Easy" :
                  type == EnemyType::Medium ? "Medium" : "Hard")
              << " enemy in lane " << lane
              << ". Active enemies: " << getActiveEnemyCount()
              << std::endl;
}

// Pick types in a predictable sequence so every wave guarantees its required mix.
EnemyType getWaveEnemyType(int wave, int enemyIndex) {
    if (wave == 1) {
        return EnemyType::Easy;
    }

    if (wave == 2) {
        // Five enemies: Easy, Medium, Easy, Medium, Easy.
        return (enemyIndex % 2 == 0) ? EnemyType::Easy : EnemyType::Medium;
    }

    // Wave 3 cycles through all three types, guaranteeing a mix of Easy,
    // Medium, and Hard enemies across the ten enemies.
    switch (enemyIndex % 3) {
        case 0: return EnemyType::Easy;
        case 1: return EnemyType::Medium;
        default: return EnemyType::Hard;
    }
}

// Move active enemies toward the home base and detect a base breach.
void updateEnemies(float deltaTime) {
    if (gameOver) {
        return;
    }

    for (Enemy& enemy : enemies) {
        if (!enemy.active) {
            continue;
        }

        enemy.x -= ENEMY_SPEED * deltaTime;
        if (enemy.x <= BASE_RIGHT_X) {
            enemy.x = BASE_RIGHT_X;
            gameOver = true;
            playerWon = false;
            std::cout << "GAME OVER: an enemy reached the home base in lane "
                      << enemy.laneIndex << "." << std::endl;
            return;
        }
    }
}

// Spawn only the current wave's required enemies. The next wave (or victory)
// is triggered only after spawning is finished and the field is clear.
void updateWaveSystem(double currentTime) {
    if (gameOver || currentWave > TOTAL_WAVES) {
        return;
    }

    const int requiredEnemies = WAVE_ENEMY_COUNTS[currentWave - 1];

    if (spawnedInCurrentWave < requiredEnemies &&
        currentTime - lastSpawnTime >= SPAWN_INTERVAL &&
        getActiveEnemyCount() < MAX_ACTIVE_ENEMIES) {
        const EnemyType type = getWaveEnemyType(
            currentWave, spawnedInCurrentWave
        );
        spawnEnemy(type);
        ++spawnedInCurrentWave;
        lastSpawnTime = currentTime;

        std::cout << "Wave " << currentWave << ": spawned "
                  << spawnedInCurrentWave << "/" << requiredEnemies
                  << " enemies." << std::endl;
    }

    if (spawnedInCurrentWave == requiredEnemies && areAllEnemiesDefeated()) {
        if (currentWave == TOTAL_WAVES) {
            playerWon = true;
            gameOver = true;
            std::cout << "VICTORY! All three waves have been completed."
                      << std::endl;
        } else {
            ++currentWave;
            spawnedInCurrentWave = 0;
            lastSpawnTime = currentTime;
            std::cout << "Wave cleared! Starting Wave " << currentWave
                      << ". Defeat all enemies to advance." << std::endl;
        }
    }
}

// Count only enemies that are still active.
int getActiveEnemyCount() {
    int count = 0;
    for (const Enemy& enemy : enemies) {
        if (enemy.active) {
            ++count;
        }
    }
    return count;
}

// Remove enemies that Person 2 has marked inactive after defeat.
void removeDefeatedEnemies() {
    enemies.erase(
        std::remove_if(
            enemies.begin(),
            enemies.end(),
            [](const Enemy& enemy) { return !enemy.active; }
        ),
        enemies.end()
    );
}

// Used by Person 1 to determine whether the field is clear.
bool areAllEnemiesDefeated() {
    return getActiveEnemyCount() == 0;
}

// Return the vertical center of a lane.
float getLaneCenterY(int laneIndex) {
    switch (laneIndex) {
        case 0: return 0.525f;
        case 1: return 0.175f;
        case 2: return -0.175f;
        case 3: return -0.525f;
        default: return 0.0f;
    }
}

// Give each enemy type a distinct display color.
void setEnemyColor(int colorLocation, EnemyType type) {
    switch (type) {
        case EnemyType::Easy:
            glUniform3f(colorLocation, 0.1f, 0.2f, 0.9f); // Blue
            break;
        case EnemyType::Medium:
            glUniform3f(colorLocation, 0.95f, 0.55f, 0.05f); // Orange
            break;
        case EnemyType::Hard:
            glUniform3f(colorLocation, 0.75f, 0.0f, 0.75f); // Purple
            break;
    }
}


// Shader sources
const char* vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

void main() {
    gl_Position = vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
uniform vec3 laneColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(laneColor, 1.0);
}
)";

// Four lane backgrounds: six vertices per lane
float laneVertices[] = {
    // Lane 0
    -0.9f,  0.7f, 0.0f,
     0.9f,  0.7f, 0.0f,
     0.9f,  0.35f, 0.0f,
    -0.9f,  0.7f, 0.0f,
     0.9f,  0.35f, 0.0f,
    -0.9f,  0.35f, 0.0f,

    // Lane 1
    -0.9f,  0.35f, 0.0f,
     0.9f,  0.35f, 0.0f,
     0.9f,  0.0f, 0.0f,
    -0.9f,  0.35f, 0.0f,
     0.9f,  0.0f, 0.0f,
    -0.9f,  0.0f, 0.0f,

    // Lane 2
    -0.9f,  0.0f, 0.0f,
     0.9f,  0.0f, 0.0f,
     0.9f, -0.35f, 0.0f,
    -0.9f,  0.0f, 0.0f,
     0.9f, -0.35f, 0.0f,
    -0.9f, -0.35f, 0.0f,

    // Lane 3
    -0.9f, -0.35f, 0.0f,
     0.9f, -0.35f, 0.0f,
     0.9f, -0.7f, 0.0f,
    -0.9f, -0.35f, 0.0f,
     0.9f, -0.7f, 0.0f,
    -0.9f, -0.7f, 0.0f
};

// Home base rectangle: two triangles
float baseVertices[] = {
    LEFT_X,       TOP_Y,    0.0f,
    BASE_RIGHT_X, TOP_Y,    0.0f,
    BASE_RIGHT_X, BOTTOM_Y, 0.0f,

    LEFT_X,       TOP_Y,    0.0f,
    BASE_RIGHT_X, BOTTOM_Y, 0.0f,
    LEFT_X,       BOTTOM_Y, 0.0f
};

// Grid lines: 5 horizontal and 3 vertical lines
float gridVertices[] = {
    // Horizontal lines
    LEFT_X, TOP_Y,       0.0f, RIGHT_X, TOP_Y,       0.0f,
    LEFT_X, LANE_1_Y,    0.0f, RIGHT_X, LANE_1_Y,    0.0f,
    LEFT_X, LANE_2_Y,    0.0f, RIGHT_X, LANE_2_Y,    0.0f,
    LEFT_X, LANE_3_Y,    0.0f, RIGHT_X, LANE_3_Y,    0.0f,
    LEFT_X, BOTTOM_Y,    0.0f, RIGHT_X, BOTTOM_Y,    0.0f,

    // Left and right borders
    LEFT_X, TOP_Y,       0.0f, LEFT_X, BOTTOM_Y,     0.0f,
    RIGHT_X, TOP_Y,      0.0f, RIGHT_X, BOTTOM_Y,    0.0f,

    // Home base divider
    BASE_RIGHT_X, TOP_Y,    0.0f,
    BASE_RIGHT_X, BOTTOM_Y, 0.0f
};

// OpenGL objects
unsigned int shaderProgram = 0;

unsigned int laneVAO = 0;
unsigned int laneVBO = 0;

unsigned int baseVAO = 0;
unsigned int baseVBO = 0;

unsigned int gridVAO = 0;
unsigned int gridVBO = 0;

unsigned int enemyVAO = 0;
unsigned int enemyVBO = 0;
float enemyVertex[] = {0.0f, 0.0f, 0.0f};

// Compile a shader and report errors
unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    char infoLog[512];

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);

        std::cerr << "Shader compilation failed:\n"
                  << infoLog << std::endl;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

// Create the shader program
bool createShaderProgram() {
    unsigned int vertexShader =
        compileShader(GL_VERTEX_SHADER, vertexShaderSrc);

    if (vertexShader == 0) {
        return false;
    }

    unsigned int fragmentShader =
        compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    char infoLog[512];

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(
            shaderProgram, 512, nullptr, infoLog
        );

        std::cerr << "Shader linking failed:\n"
                  << infoLog << std::endl;

        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return success;
}

// Configure a VAO/VBO pair
void setupGeometry(unsigned int& VAO, unsigned int& VBO,
                   float* vertices, size_t size) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        size,
        vertices,
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        3 * sizeof(float), (void*)0
    );

    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

// Update viewport when the window is resized
void framebuffer_size_callback(GLFWwindow* window,
                               int width, int height) {
    glViewport(0, 0, width, height);
}

// Mouse callback: select a lane with the first valid click
void mouse_button_callback(GLFWwindow* window, int button,
                           int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT ||
        action != GLFW_PRESS ||
        selectedLane != -1) {
        return;
    }

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    if (windowWidth <= 0 || windowHeight <= 0) {
        return;
    }

    // Convert window coordinates to OpenGL coordinates
    float xOpenGL =
        2.0f * static_cast<float>(mouseX) / windowWidth - 1.0f;

    float yOpenGL =
        1.0f - 2.0f * static_cast<float>(mouseY) / windowHeight;

    // Reject clicks outside the grid
    if (xOpenGL < LEFT_X || xOpenGL > RIGHT_X ||
        yOpenGL < BOTTOM_Y || yOpenGL > TOP_Y) {
        return;
    }

    // Lane 0 includes the top boundary
    if (yOpenGL <= TOP_Y && yOpenGL > LANE_1_Y) {
        selectedLane = 0;
    }
    // Lane 1
    else if (yOpenGL <= LANE_1_Y && yOpenGL > LANE_2_Y) {
        selectedLane = 1;
    }
    // Lane 2
    else if (yOpenGL <= LANE_2_Y && yOpenGL > LANE_3_Y) {
        selectedLane = 2;
    }
    // Lane 3 includes the bottom boundary
    else if (yOpenGL <= LANE_3_Y && yOpenGL >= BOTTOM_Y) {
        selectedLane = 3;
    }

    if (selectedLane != -1) {
        std::cout << "Selected lane: "
                  << selectedLane << std::endl;
    }
}

// TEMPORARY TEST CONTROL: press X to defeat the first active enemy.
// Replace this with the real combat/defeat logic when it is implemented.
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key != GLFW_KEY_X || action != GLFW_PRESS || gameOver) {
        return;
    }

    for (Enemy& enemy : enemies) {
        if (enemy.active) {
            enemy.active = false;
            break;
        }
    }

    removeDefeatedEnemies();
    std::cout << "Active enemies: " << getActiveEnemyCount()
              << " | Field clear: "
              << (areAllEnemiesDefeated() ? "Yes" : "No")
              << std::endl;
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(
        GLFW_OPENGL_PROFILE,
        GLFW_OPENGL_CORE_PROFILE
    );

    // Create window
    GLFWwindow* window = glfwCreateWindow(
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        "Game - Home Base and Lanes",
        nullptr,
        nullptr
    );

    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Initialize GLAD
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;

        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Register callbacks
    glfwSetFramebufferSizeCallback(
        window, framebuffer_size_callback
    );

    glfwSetMouseButtonCallback(
        window, mouse_button_callback
    );
    glfwSetKeyCallback(window, key_callback);


    // Create shaders
    if (!createShaderProgram()) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Upload lane geometry
    setupGeometry(
        laneVAO, laneVBO,
        laneVertices, sizeof(laneVertices)
    );

    // Upload home base geometry
    setupGeometry(
        baseVAO, baseVBO,
        baseVertices, sizeof(baseVertices)
    );

    // Upload grid geometry
    setupGeometry(
        gridVAO, gridVBO,
        gridVertices, sizeof(gridVertices)
    );

    // Enemy display geometry (one point, updated for each enemy).
    setupGeometry(enemyVAO, enemyVBO, enemyVertex, sizeof(enemyVertex));

    // Seed the random number generator once for random lane assignment.
    srand(static_cast<unsigned int>(time(nullptr)));

    // Start the first wave's spawn timer.
    lastSpawnTime = glfwGetTime();
    previousFrameTime = lastSpawnTime;

    // Set initial viewport using framebuffer dimensions
    int framebufferWidth, framebufferHeight;

    glfwGetFramebufferSize(
        window, &framebufferWidth, &framebufferHeight
    );

    glViewport(0, 0, framebufferWidth, framebufferHeight);

    // Find shader color uniform
    int colorLocation =
        glGetUniformLocation(shaderProgram, "laneColor");

    if (colorLocation == -1) {
        std::cerr << "Could not find laneColor uniform"
                  << std::endl;
    }

    std::cout << "Wave system started: Wave 1 has 3 Easy enemies; "
              << "Wave 2 has 5 Easy/Medium enemies; Wave 3 has 10 "
              << "Easy/Medium/Hard enemies. Press X to defeat one enemy "
              << "(temporary test)." << std::endl;

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        const double currentTime = glfwGetTime();
        const float deltaTime = static_cast<float>(
            currentTime - previousFrameTime
        );
        previousFrameTime = currentTime;

        if (!gameOver) {
            updateEnemies(deltaTime);
            updateWaveSystem(currentTime);
        }

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // 1. Draw the four lane backgrounds
        glBindVertexArray(laneVAO);

        for (int lane = 0; lane < 4; lane++) {
            if (lane == selectedLane) {
                // Selected lane: dark green
                glUniform3f(
                    colorLocation, 0.0f, 0.35f, 0.0f
                );
            } else {
                // Normal lane: light green
                glUniform3f(
                    colorLocation, 0.3f, 0.8f, 0.3f
                );
            }

            glDrawArrays(GL_TRIANGLES, lane * 6, 6);
        }

        // 2. Draw the home base
        glBindVertexArray(baseVAO);

        // Dark red
        glUniform3f(
            colorLocation, 0.55f, 0.08f, 0.08f
        );

        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 3. Draw active enemies as colored points.
        glBindVertexArray(enemyVAO);
        glBindBuffer(GL_ARRAY_BUFFER, enemyVBO);
        glPointSize(18.0f);

        for (const Enemy& enemy : enemies) {
            if (!enemy.active) {
                continue;
            }

            float vertex[] = {
                enemy.x,
                getLaneCenterY(enemy.laneIndex),
                0.0f
            };

            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertex), vertex);
            setEnemyColor(colorLocation, enemy.type);
            glDrawArrays(GL_POINTS, 0, 1);
        }

        // 4. Draw grid lines last
        glBindVertexArray(gridVAO);

        glUniform3f(
            colorLocation, 0.0f, 0.0f, 0.0f
        );

        glDrawArrays(GL_LINES, 0, 16);

        // Display frame
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean up OpenGL resources
    glDeleteVertexArrays(1, &laneVAO);
    glDeleteBuffers(1, &laneVBO);

    glDeleteVertexArrays(1, &baseVAO);
    glDeleteBuffers(1, &baseVBO);

    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

    glDeleteVertexArrays(1, &enemyVAO);
    glDeleteBuffers(1, &enemyVBO);

    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
