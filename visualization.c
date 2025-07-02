#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

#define DEFAULT_SCREEN_WIDTH 1200
#define DEFAULT_SCREEN_HEIGHT 800
#define MANUAL_TIME_STEP 0.05

// Color definitions
#define COLOR_BLACK_V4F ((V4f){0.0f, 0.0f, 0.0f, 1.0f})
#define COLOR_WHITE_V4F ((V4f){1.0f, 1.0f, 1.0f, 1.0f})
#define COLOR_PINK_V4F ((V4f){0.73f, 0.67f, 1.0f, 1.0f})
#define COLOR_GREEN_V4F ((V4f){0.0f, 1.0f, 0.0f, 1.0f})

// Simple vector types
typedef struct { float x, y; } V2f;
typedef struct { float x, y, z, w; } V4f;

V2f v2f(float x, float y) { return (V2f){x, y}; }
V4f v4f(float x, float y, float z, float w) { return (V4f){x, y, z, w}; }
V2f v2f_add(V2f a, V2f b) { return v2f(a.x + b.x, a.y + b.y); }
V2f v2f_sub(V2f a, V2f b) { return v2f(a.x - b.x, a.y - b.y); }
V2f v2f_scale(V2f a, float s) { return v2f(a.x * s, a.y * s); }

// Rule 110 cellular automaton definitions
#define ROWS 100
#define COLS 120
#define CELL_SIZE 8.0f

typedef enum {
    O = 0,
    I = 1
} Cell;

// Rule 110 patterns
Cell patterns[8] = {
    [0b000] = O, [0b001] = I, [0b010] = I, [0b011] = I,
    [0b100] = O, [0b101] = I, [0b110] = I, [0b111] = O,
};

typedef struct {
    Cell cells[COLS];
} Row;

typedef struct {
    Row rows[ROWS];
    int current_row;
    int generation;
} Board;

// OpenGL types
typedef struct {
    V2f pos;
    V2f uv;
    V4f color;
} Vertex;

#define VERTEX_BUF_CAP (128 * 1024)
typedef struct {
    bool reload_failed;
    GLuint vao;
    GLuint vbo;
    GLuint program;
    GLint uniforms[4]; // resolution, time, mouse, tex
    size_t vertex_buf_sz;
    Vertex vertex_buf[VERTEX_BUF_CAP];
} Renderer;

// Global state
static Board board = {0};
static Renderer renderer = {0};
static double time_accumulator = 0.0;
static double generation_time = 0.15; // Time between generations
static bool paused = false;
static bool show_grid = true;

// GL extension function pointers
static void (*glGenVertexArrays)(GLsizei n, GLuint *arrays) = NULL;
static void (*glBindVertexArray)(GLuint array) = NULL;

// Simple GL extension loader
void load_gl_extensions() {
    glGenVertexArrays = (void(*)(GLsizei, GLuint*))glfwGetProcAddress("glGenVertexArrays");
    glBindVertexArray = (void(*)(GLuint))glfwGetProcAddress("glBindVertexArray");
}

void panic_errno(const char *fmt, ...) {
    fprintf(stderr, "ERROR: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, ": %s\n", strerror(errno));
    exit(1);
}

char *slurp_file(const char *file_path) {
    FILE *f = fopen(file_path, "r");
    if (f == NULL) {
        panic_errno("Could not read file %s", file_path);
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buffer = malloc(size + 1);
    if (buffer == NULL) {
        panic_errno("Could not allocate memory for file %s", file_path);
    }
    
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);
    return buffer;
}

bool compile_shader_source(const char *source, GLenum shader_type, GLuint *shader) {
    *shader = glCreateShader(shader_type);
    glShaderSource(*shader, 1, &source, NULL);
    glCompileShader(*shader);

    GLint compiled = 0;
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        GLchar message[1024];
        GLsizei message_size = 0;
        glGetShaderInfoLog(*shader, sizeof(message), &message_size, message);
        fprintf(stderr, "ERROR: could not compile shader\n");
        fprintf(stderr, "%.*s\n", message_size, message);
        return false;
    }
    return true;
}

bool link_program(GLuint vert_shader, GLuint frag_shader, GLuint *program) {
    *program = glCreateProgram();
    glAttachShader(*program, vert_shader);
    glAttachShader(*program, frag_shader);
    glLinkProgram(*program);

    GLint linked = 0;
    glGetProgramiv(*program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLsizei message_size = 0;
        GLchar message[1024];
        glGetProgramInfoLog(*program, sizeof(message), &message_size, message);
        fprintf(stderr, "Program Linking Error: %.*s\n", message_size, message);
        return false;
    }

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
    return true;
}

bool load_shader_program(Renderer *r) {
    const char *vertex_source = 
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec2 aUV;\n"
        "layout (location = 2) in vec4 aColor;\n"
        "uniform vec2 resolution;\n"
        "out vec2 fragUV;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    vec2 screen_pos = (aPos / resolution) * 2.0 - 1.0;\n"
        "    screen_pos.y = -screen_pos.y;\n"
        "    gl_Position = vec4(screen_pos, 0.0, 1.0);\n"
        "    fragUV = aUV;\n"
        "    fragColor = aColor;\n"
        "}\n";

    const char *fragment_source = 
        "#version 330 core\n"
        "in vec2 fragUV;\n"
        "in vec4 fragColor;\n"
        "out vec4 finalColor;\n"
        "void main() {\n"
        "    finalColor = fragColor;\n"
        "}\n";

    GLuint vert_shader, frag_shader;
    if (!compile_shader_source(vertex_source, GL_VERTEX_SHADER, &vert_shader)) {
        return false;
    }
    if (!compile_shader_source(fragment_source, GL_FRAGMENT_SHADER, &frag_shader)) {
        return false;
    }
    if (!link_program(vert_shader, frag_shader, &r->program)) {
        return false;
    }

    glUseProgram(r->program);
    r->uniforms[0] = glGetUniformLocation(r->program, "resolution");
    r->uniforms[1] = glGetUniformLocation(r->program, "time");
    r->uniforms[2] = glGetUniformLocation(r->program, "mouse");
    r->uniforms[3] = glGetUniformLocation(r->program, "tex");

    return true;
}

// Rule 110 functions
Row next_row(Row prev) {
    Row next = {0};
    for (int i = 0; i < COLS; ++i) {
        int left = (i == 0) ? 0 : prev.cells[i-1];
        int center = prev.cells[i];
        int right = (i == COLS-1) ? 0 : prev.cells[i+1];
        int pattern_index = (left << 2) | (center << 1) | right;
        next.cells[i] = patterns[pattern_index];
    }
    return next;
}

Row random_row(void) {
    Row result = {0};
    // Start with a single cell in the middle
    result.cells[COLS/2] = I;
    // Or add some randomness
    for (int i = 0; i < COLS; ++i) {
        if (rand() % 100 < 5) { // 5% chance
            result.cells[i] = I;
        }
    }
    return result;
}

void board_init(Board *board) {
    memset(board, 0, sizeof(*board));
    board->rows[0] = random_row();
    board->current_row = 0;
    board->generation = 0;
}

void board_next_generation(Board *board) {
    if (board->current_row < ROWS - 1) {
        board->current_row++;
        board->rows[board->current_row] = next_row(board->rows[board->current_row - 1]);
        board->generation++;
    } else {
        // Scroll up - shift all rows up and generate new bottom row
        for (int i = 0; i < ROWS - 1; ++i) {
            board->rows[i] = board->rows[i + 1];
        }
        board->rows[ROWS - 1] = next_row(board->rows[ROWS - 2]);
        board->generation++;
    }
}

// Rendering functions
void r_vertex(Renderer *r, V2f pos, V2f uv, V4f color) {
    if (r->vertex_buf_sz >= VERTEX_BUF_CAP) {
        fprintf(stderr, "WARNING: Vertex buffer overflow! Skipping vertices.\n");
        return;
    }
    r->vertex_buf[r->vertex_buf_sz].pos = pos;
    r->vertex_buf[r->vertex_buf_sz].uv = uv;
    r->vertex_buf[r->vertex_buf_sz].color = color;
    r->vertex_buf_sz += 1;
}

void r_quad(Renderer *r, V2f p1, V2f p2, V4f color) {
    V2f a = p1;
    V2f b = v2f(p2.x, p1.y);
    V2f c = v2f(p1.x, p2.y);
    V2f d = p2;

    // First triangle
    r_vertex(r, a, v2f(0.0f, 0.0f), color);
    r_vertex(r, b, v2f(1.0f, 0.0f), color);
    r_vertex(r, c, v2f(0.0f, 1.0f), color);

    // Second triangle
    r_vertex(r, b, v2f(1.0f, 0.0f), color);
    r_vertex(r, c, v2f(0.0f, 1.0f), color);
    r_vertex(r, d, v2f(1.0f, 1.0f), color);
}

void r_line(Renderer *r, V2f p1, V2f p2, V4f color, float thickness) {
    V2f dir = v2f_sub(p2, p1);
    V2f norm = v2f(-dir.y, dir.x);
    float len = sqrtf(norm.x * norm.x + norm.y * norm.y);
    if (len > 0.0f) {
        norm = v2f_scale(norm, thickness / (2.0f * len));
    }

    V2f a = v2f_add(p1, norm);
    V2f b = v2f_sub(p1, norm);
    V2f c = v2f_add(p2, norm);
    V2f d = v2f_sub(p2, norm);

    r_vertex(r, a, v2f(0.0f, 0.0f), color);
    r_vertex(r, b, v2f(0.0f, 1.0f), color);
    r_vertex(r, c, v2f(1.0f, 0.0f), color);

    r_vertex(r, b, v2f(0.0f, 1.0f), color);
    r_vertex(r, c, v2f(1.0f, 0.0f), color);
    r_vertex(r, d, v2f(1.0f, 1.0f), color);
}

void r_sync_buffers(Renderer *r) {
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * r->vertex_buf_sz, r->vertex_buf);
}

void r_clear(Renderer *r) {
    r->vertex_buf_sz = 0;
}

void board_render(Renderer *r, const Board *board, int width, int height) {
    float cell_width = (float)width / COLS;
    float cell_height = (float)height / ROWS;
    
    // Render cells first
    int max_row = (board->current_row < ROWS - 1) ? board->current_row : ROWS - 1;
    for (int row = 0; row <= max_row; ++row) {
        for (int col = 0; col < COLS; ++col) {
            if (board->rows[row].cells[col] == I) {
                float x = col * cell_width;
                float y = row * cell_height;
                r_quad(r, 
                       v2f(x, y), 
                       v2f(x + cell_width, y + cell_height), 
                       COLOR_PINK_V4F);
            }
        }
    }

    // Render grid if enabled (optimized - only render visible area)
    if (show_grid && cell_width > 2.0f && cell_height > 2.0f) {
        V4f grid_color = v4f(0.2f, 0.2f, 0.2f, 0.3f);
        
        // Vertical lines (skip some if too dense)
        int col_step = (cell_width < 4.0f) ? 5 : 1;
        for (int col = 0; col <= COLS; col += col_step) {
            float x = col * cell_width;
            r_line(r, v2f(x, 0), v2f(x, height), grid_color, 1.0f);
        }
        
        // Horizontal lines (skip some if too dense)
        int row_step = (cell_height < 4.0f) ? 5 : 1;
        for (int row = 0; row <= max_row; row += row_step) {
            float y = row * cell_height;
            r_line(r, v2f(0, y), v2f(width, y), grid_color, 1.0f);
        }
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void) scancode; (void) mods;

    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_SPACE:
                paused = !paused;
                break;
            case GLFW_KEY_R:
                board_init(&board);
                break;
            case GLFW_KEY_G:
                show_grid = !show_grid;
                break;
            case GLFW_KEY_UP:
                generation_time = fmax(0.01, generation_time - 0.01);
                break;
            case GLFW_KEY_DOWN:
                generation_time = fmin(1.0, generation_time + 0.01);
                break;
            case GLFW_KEY_ESCAPE:
            case GLFW_KEY_Q:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;
        }
        
        if (paused && key == GLFW_KEY_RIGHT) {
            board_next_generation(&board);
        }
    }
}

void r_init(Renderer *r) {
    if (glGenVertexArrays == NULL || glBindVertexArray == NULL) {
        fprintf(stderr, "ERROR: Required OpenGL extensions not available\n");
        exit(1);
    }
    
    glGenVertexArrays(1, &r->vao);
    glBindVertexArray(r->vao);

    glGenBuffers(1, &r->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(r->vertex_buf), r->vertex_buf, GL_DYNAMIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // UV attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));

    // Color attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(4 * sizeof(float)));
}

int main() {
    if (!glfwInit()) {
        fprintf(stderr, "Could not initialize GLFW\n");
        exit(1);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT, 
                                          "Rule 110 Cellular Automaton", NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Could not create a window\n");
        glfwTerminate();
        exit(1);
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    load_gl_extensions();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    srand(time(NULL));
    
    // Initialize renderer and board
    r_init(&renderer);
    if (!load_shader_program(&renderer)) {
        fprintf(stderr, "Failed to load shaders\n");
        exit(1);
    }
    board_init(&board);

    printf("Controls:\n");
    printf("  SPACE - Pause/Resume\n");
    printf("  R - Reset\n");
    printf("  G - Toggle Grid\n");
    printf("  UP/DOWN - Speed control\n");
    printf("  RIGHT - Step (when paused)\n");
    printf("  Q/ESC - Quit\n");

    double prev_time = glfwGetTime();
    
    while (!glfwWindowShouldClose(window)) {
        double current_time = glfwGetTime();
        double delta_time = current_time - prev_time;
        prev_time = current_time;

        // Update simulation
        if (!paused) {
            time_accumulator += delta_time;
            while (time_accumulator >= generation_time) {
                board_next_generation(&board);
                time_accumulator -= generation_time;
            }
        }

        // Get window size
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        // Clear screen
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render
        glUseProgram(renderer.program);
        glUniform2f(renderer.uniforms[0], (float)width, (float)height);
        
        r_clear(&renderer);
        board_render(&renderer, &board, width, height);
        r_sync_buffers(&renderer);
        
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)renderer.vertex_buf_sz);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
