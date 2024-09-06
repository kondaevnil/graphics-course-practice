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

std::string to_string(std::string_view str)
{
    return {str.begin(), str.end()};
}

void sdl2_fail(std::string_view message)
{
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error)
{
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const char fragment_source[] =
R"(#version 330 core

layout (location = 0) out vec4 out_color;
//in vec3 color;
flat in vec3 color;
in vec2 coord;
void main()
{
// vec4(R, G, B, A)
    float size = 20;
    float c = mod((floor(size * coord.x)+floor(size * coord.y)), 2.0);
    out_color = vec4(c, c, c, 1.0);
}
)";

const char vertex_source[] =
R"(#version 330 core
const vec2 VERTICES[3] = vec2[3](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0)
);
//out vec3 color;
flat out vec3 color;
out vec2 coord;
void main()
{
    gl_Position = vec4(VERTICES[gl_VertexID], 0.0, 1.0);
    color = vec3(gl_Position.x, gl_Position.y, gl_Position.x * gl_Position.y);
    coord = vec2(gl_Position.x, gl_Position.y);
}
)";

GLuint create_shader(GLenum shader_type,
                     const char *shader_source) {
    auto shader_id = glCreateShader(shader_type);

    if (shader_id == 0) {
        throw std::runtime_error("Shader creating error");
    }

    glShaderSource(shader_id, 1, &shader_source, NULL);
    glCompileShader(shader_id);

    GLint status;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &status);

    if (status == GL_FALSE) {
        GLint info_log_len;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_log_len);
        std::string info_log(info_log_len, '\0');
        GLint written_log_len;
        glGetShaderInfoLog(shader_id, info_log_len, &written_log_len, info_log.data());

        throw std::runtime_error(info_log);
    }

    return shader_id;
}

GLuint create_program(GLuint vertex_shader,
                      GLuint fragment_shader) {
    GLuint program_id = glCreateProgram();

    if (program_id == 0) {
        throw std::runtime_error(" error occurs creating the program object\n");
    }

    glAttachShader(program_id, vertex_shader);
    glAttachShader(program_id, fragment_shader);

    glLinkProgram(program_id);

    GLint link_status;
    glGetProgramiv(program_id, GL_LINK_STATUS, &link_status);

    if (link_status == GL_FALSE) {
        GLint info_log_len;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_len);
        std::string info_log(info_log_len, '\0');
        GLint written_log_len;
        glGetProgramInfoLog(program_id, info_log_len, &written_log_len, info_log.data());

        throw std::runtime_error(info_log);
    }

    return program_id;
}

int main() try
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_Window *window = SDL_CreateWindow("Graphics course practice 1",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    GLuint vert_id = create_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint frag_id = create_shader(GL_FRAGMENT_SHADER, fragment_source);

    GLuint program_id = create_program(vert_id, frag_id);

    GLuint va;
    glGenVertexArrays(1, &va);

    bool running = true;
    while (running)
    {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        }

        if (!running)
            break;

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program_id);
        glBindVertexArray(va);
        glDrawArrays(GL_TRIANGLES, 0, 3);

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
