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
#include <map>

#include "obj_parser.hpp"


namespace mth
{
    template<typename Type = float>
    class matr
    {
    private:
        Type A[4][4];

    public:
        matr()
        {
        }

        matr( const Type *M )
        {
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                    A[i][j] = M[i][j];
        }

        matr( Type A00, Type A01, Type A02, Type A03,
              Type A10, Type A11, Type A12, Type A13,
              Type A20, Type A21, Type A22, Type A23,
              Type A30, Type A31, Type A32, Type A33 )
        {
            A[0][0] = A00, A[0][1] = A01, A[0][2] = A02, A[0][3] = A03;
            A[1][0] = A10, A[1][1] = A11, A[1][2] = A12, A[1][3] = A13;
            A[2][0] = A20, A[2][1] = A21, A[2][2] = A22, A[2][3] = A23;
            A[3][0] = A30, A[3][1] = A31, A[3][2] = A32, A[3][3] = A33;
        }

        static matr Identity()
        {
            return matr(1, 0, 0, 0,
                        0, 1, 0, 0,
                        0, 0, 1, 0,
                        0, 0, 0, 1);
        }

        static matr RotateZ( Type a )
        {
            float c = cos(a), s = sin(a);
            return matr(c, s, 0, 0,
                        -s, c, 0, 0,
                        0, 0, 1, 0,
                        0, 0, 0, 1);
        }

        static matr RotateY( Type a )
        {
            float c = cos(a), s = sin(a);
            return matr(c, 0, -s, 0,
                        0, 1, 0, 0,
                        s, 0, c, 0,
                        0, 0, 0, 1);
        }

        static matr RotateX( Type a )
        {
            float c = cos(a), s = sin(a);
            return matr(1, 0, 0, 0,
                        0, c, s, 0,
                        0, -s, c, 0,
                        0, 0, 0, 1);
        }

        static matr Translate( float x, float y, float z )
        {
            matr m = Identity();
            m.A[0][3] = x;
            m.A[1][3] = y;
            m.A[2][3] = z;
            return m;
        }

        static matr Scale( float s )
        {
            matr m = Identity();
            m.A[0][0] = s;
            m.A[1][1] = s;
            m.A[2][2] = s;
            return m;
        }

        matr<Type> operator*( const matr<Type> m1 ) const
        {
            matr<Type> m;
            int k;

            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                    for (k = 0, m.A[i][j] = 0; k < 4; k++)
                        m.A[i][j] += A[i][k] * m1.A[k][j];
            return m;
        }

        operator Type *(void)
        {
            return A[0];
        }

        operator const Type *(void) const
        {
            return A[0];
        }

        static matr<Type> Frustum( Type right, Type top, Type near, Type far )
        {
            return matr<Type>( near / right * 1.f, 0.f,              0.f,                                0.f,
                               0.f,                near / top * 1.f, 0.f,                                0.f,
                               0.f,                0.f,              -(far + near) / (far - near) * 1.f, -2 * far * near / (far - near),
                               0.f,                0.f,              -1.f,                               1.f);
        }
    };
}

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

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;

out vec3 normal;

void main()
{
    gl_Position = projection * view * model * vec4(in_position, 1.0);
    normal = normalize(mat3(model) * in_normal);
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

in vec3 normal;

layout (location = 0) out vec4 out_color;

void main()
{
    vec3 ambient_dir = vec3(0.0, 1.0, 0.0);
    vec3 ambient_color = vec3(0.2);

    vec3 light1_dir = normalize(vec3( 3.0, 2.0,  1.0));
    vec3 light2_dir = normalize(vec3(-3.0, 2.0, -1.0));

    vec3 light1_color = vec3(1.0,  0.5, 0.25);
    vec3 light2_color = vec3(0.25, 0.5, 1.0 );

    vec3 n = normalize(normal);

    vec3 color = (0.5 + 0.5 * dot(n, ambient_dir)) * ambient_color
        + max(0.0, dot(n, light1_dir)) * light1_color
        + max(0.0, dot(n, light2_dir)) * light2_color
        ;

    float gamma = 1.0 / 2.2;
    out_color = vec4(pow(min(vec3(1.0), color), vec3(gamma)), 1.0);
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

class bunny {
public:
    obj_data *model_data;
    GLuint vao;
    GLuint vbo;
    GLuint ebo;

    float bunny_x = 0;
    float bunny_y = 0;
    float speed = 10;

    mth::matr<float> transform = mth::matr<float>::Identity();

    bunny(obj_data *bunny) : model_data(bunny) {
        glGenVertexArrays(1, &vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, bunny->vertices.size() * sizeof(bunny->vertices[0]), bunny->vertices.data(), GL_STATIC_DRAW);

        glBindVertexArray(vao);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void*)(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void*)(12));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void*)(24));

        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, bunny->indices.size() * sizeof(bunny->indices[0]), bunny->indices.data(), GL_STATIC_DRAW);
    }

    void response(float dt, std::map<SDL_Keycode, bool> &button_down) {
        if (button_down[SDLK_LEFT]) {
            bunny_x -= speed * dt;
        }
        if (button_down[SDLK_RIGHT]) {
            bunny_x += speed * dt;
        }
        if (button_down[SDLK_UP]) {
            bunny_y += speed * dt;
        }
        if (button_down[SDLK_DOWN]) {
            bunny_y -= speed * dt;
        }
    }
};

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
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 4",
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

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.1f, 0.1f, 0.2f, 0.f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
//    glCullFace(GL_FRONT);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);

    GLint model_location = glGetUniformLocation(program, "model");
    GLint view_location = glGetUniformLocation(program, "view");
    GLint projection_location = glGetUniformLocation(program, "projection");

    std::string project_root = PROJECT_ROOT;
    obj_data bunny_data = parse_obj(project_root + "/bunny.obj");

    std::vector<bunny> obj;
    obj.emplace_back(&bunny_data);
    obj.emplace_back(&bunny_data);
    obj.emplace_back(&bunny_data);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;
    float near = 0.01f;
    float far = 140.f;
    float right = near;
    float top = right * static_cast<float>(height) / static_cast<float>(width);
    float scale = 0.5;

    std::map<SDL_Keycode, bool> button_down;

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
        case SDL_KEYDOWN:
            button_down[event.key.keysym.sym] = true;
            break;
        case SDL_KEYUP:
            button_down[event.key.keysym.sym] = false;
            break;
        }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto v = mth::matr(
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, -3.f,
            0.f, 0.f, 0.f, 1.f
        );
        auto p = mth::matr<float>::Frustum(right, top, near, far);
        auto vp = p * v;

        glUseProgram(program);

        obj[0].transform = mth::matr<float>::RotateX(time * 1);
        obj[1].transform = mth::matr<float>::RotateY(time * 2);
        obj[2].transform = mth::matr<float>::RotateZ(time * 4);
        int cnt = -1;
        for (auto &b: obj) {
            b.response(dt, button_down);
            b.transform = mth::matr<float>::Translate(b.bunny_x + cnt++, b.bunny_y, 0) * mth::matr<float>::Scale(scale) * b.transform;
            auto wvp = vp * b.transform; // теперь можно передавать лишь одну матрицу на шейдер

            glBindVertexArray(b.vao);
            glUniformMatrix4fv(model_location, 1, GL_TRUE, b.transform);
            glUniformMatrix4fv(view_location, 1, GL_TRUE, v);
            glUniformMatrix4fv(projection_location, 1, GL_TRUE, p);
            glDrawElements(GL_TRIANGLES, b.model_data->indices.size(), GL_UNSIGNED_INT, (void*)(0));

            b.transform = mth::matr<float>::Identity();
        }

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
