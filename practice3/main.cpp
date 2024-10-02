#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>

std::string to_string(std::string_view str)
{
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message)
{
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error)
{
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 view;
uniform float is_bez;
uniform float time;

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec4 in_color;
layout (location = 2) in float in_dist;

out vec4 color;
out float bez;
out float dist;

void main()
{
    gl_Position = view * vec4(in_position, 0.0, 1.0);
    color = in_color;
    bez = is_bez;
    dist = in_dist + mod(time * 100, 40.0);
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

in vec4 color;
in float bez;
in float dist;

layout (location = 0) out vec4 out_color;

void main()
{
    if (bez > 0 && mod(dist, 40.0) < 20.0) {
        discard;
    }

    out_color = color;
}
)";

GLuint create_shader(GLenum type, const char * source)
{
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

struct vec2
{
    float x;
    float y;
};

struct vertex
{
    vec2 position;
    std::uint8_t color[4];
};

std::vector<vertex> getArray(int w, int h) {
    std::vector<vertex> v(3);
    for (int i = 0; i < 3; i++) {
//        v[i].position = {static_cast<float>(i - 1) / 3, static_cast<float>(i % 2) / 3};
//        v[i].color[i] = UCHAR_MAX;
//        v[i].color[3] = UCHAR_MAX;
        v[i].position = {
                (static_cast<float>(i - 1) / 3 + 1) * w / 2,
                (1 - static_cast<float>(i % 2) / 3) * h / 2
        };
        v[i].color[i] = UCHAR_MAX;
        v[i].color[3] = UCHAR_MAX;
    }

    return v;
}

vec2 bezier(std::vector<vertex> const & vertices, float t)
{
    std::vector<vec2> points(vertices.size());

    for (std::size_t i = 0; i < vertices.size(); ++i)
        points[i] = vertices[i].position;

    // De Casteljau's algorithm
    for (std::size_t k = 0; k + 1 < vertices.size(); ++k) {
        for (std::size_t i = 0; i + k + 1 < vertices.size(); ++i) {
            points[i].x = points[i].x * (1.f - t) + points[i + 1].x * t;
            points[i].y = points[i].y * (1.f - t) + points[i + 1].y * t;
        }
    }
    return points[0];
}

GLuint create_vbo(GLenum target, std::vector<vertex> &vert, GLenum usage) {
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(target, vbo);
    glBufferData(target, vert.size() * sizeof(vert[0]), vert.data(), usage);
//    {
//        vec2 coord{};
//        glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(coord), &coord);
//        std::cout << "x: " << coord.x << " y: " << coord.y << std::endl;
//    }

    return vbo;
}

GLuint create_vao() {
    GLuint vao;
    glGenVertexArrays(1, &vao);
    return vao;
}

void calc_bez(GLuint vbo, std::vector<vertex> &bez, int quality, std::vector<vertex> &pts) {
    bez.clear();
    float t = 0;
    int cnt = (static_cast<int>(pts.size()) - 1) * quality;
    for (int i = 0; i <= cnt; i++) {
        bez.push_back({bezier(pts, t), {255, 0, 0, 1}});
        t += 1.f / cnt;
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, bez.size() * sizeof(bez[0]), bez.data(), GL_DYNAMIC_DRAW);
}

void calc_dst(GLuint vbo, std::vector<vertex> &bez, std::vector<float> &dst) {
    dst.clear();
    dst.push_back(0);

    for (int i = 1; i < bez.size(); i++) {
        dst.push_back(dst[i - 1] + std::hypot(bez[i - 1].position.x - bez[i].position.x, bez[i - 1].position.y - bez[i].position.y));
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, dst.size() * sizeof(dst[0]), dst.data(), GL_DYNAMIC_DRAW);
}

int main() try
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 3",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    SDL_GL_SetSwapInterval(0);

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);
    glLineWidth(10.f);
    glPointSize(5.f);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);

    auto vert = getArray(width, height);
    GLuint trg_vao = create_vao();
    GLuint trg_vbo = create_vbo(GL_ARRAY_BUFFER, vert, GL_STATIC_DRAW);
    glBindVertexArray(trg_vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void *)(8));

    std::vector<vertex> pts;
    GLuint pts_vao = create_vao();
    GLuint pts_vbo = create_vbo(GL_ARRAY_BUFFER, pts, GL_DYNAMIC_DRAW);
    glBindVertexArray(pts_vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void *)(8));

    std::vector<vertex> bez;
    GLuint bez_vao = create_vao();
    GLuint bez_vbo = create_vbo(GL_ARRAY_BUFFER, bez, GL_DYNAMIC_DRAW);

    glBindVertexArray(bez_vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void *)(8));

    std::vector<float> dst;
    GLuint dst_vbo;
    glGenBuffers(1, &dst_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, dst_vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, dst.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void *)(0));

    int quality = 4;

    GLint view_location = glGetUniformLocation(program, "view");
    GLint is_bez_location = glGetUniformLocation(program, "is_bez");
    GLint time_location = glGetUniformLocation(program, "time");

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    bool running = true;
    while (running)
    {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_WINDOWEVENT: switch (event.window.event)
            {
            case SDL_WINDOWEVENT_RESIZED:
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                float mouse_x = event.button.x;
                float mouse_y = event.button.y;
                pts.push_back({{mouse_x, mouse_y}, {47, 7, 102, 1}});
                glBindBuffer(GL_ARRAY_BUFFER, pts_vbo);
                glBufferData(GL_ARRAY_BUFFER, pts.size() * sizeof(vert[0]), pts.data(), GL_DYNAMIC_DRAW);

                calc_bez(bez_vbo, bez, quality, pts);
                calc_dst(dst_vbo, bez, dst);
            }
            else if (event.button.button == SDL_BUTTON_RIGHT)
            {
                if (pts.size() > 0) {
                    pts.pop_back();
                    glBindBuffer(GL_ARRAY_BUFFER, pts_vbo);
                    glBufferData(GL_ARRAY_BUFFER, pts.size() * sizeof(vert[0]), pts.data(), GL_DYNAMIC_DRAW);

                    calc_bez(bez_vbo, bez, quality, pts);
                    calc_dst(dst_vbo, bez, dst);
                }
            }
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_LEFT)
            {
                quality = std::max(0, quality - 1);
                calc_bez(bez_vbo, bez, quality, pts);
                calc_dst(dst_vbo, bez, dst);
            }
            else if (event.key.keysym.sym == SDLK_RIGHT)
            {
                calc_bez(bez_vbo, bez, ++quality, pts);
                calc_dst(dst_vbo, bez, dst);
            }
            break;
        }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        glClear(GL_COLOR_BUFFER_BIT);

//        float view[16] =
//        {
//                1.f, 0.f, 0.f,  0.f,
//                0.f, 1.f, 0.f,  0.f,
//                0.f, 0.f, 1.f,  0.f,
//                0.f, 0.f, 0.f,  1.f,
//        };

        float view[16] =
        {
            2.f / width, 0.f,          0.f, -1.f,
            0.f,        -2.f / height, 0.f,  1.f,
            0.f,        0.f,           1.f,  0.f,
            0.f,        0.f,           0.f,  1.f,
        };

        glUseProgram(program);
        glBindVertexArray(trg_vao);
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);
        glUniform1f(time_location, time);
        glUniform1f(is_bez_location, -1);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindVertexArray(pts_vao);
        glDrawArrays(GL_LINE_STRIP, 0, pts.size());
        glDrawArrays(GL_POINTS, 0, pts.size());

        glBindVertexArray(bez_vao);
        glUniform1f(is_bez_location, 1);
        glDrawArrays(GL_LINE_STRIP, 0, bez.size());

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
