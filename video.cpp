#include "video.hpp"
#include "json.hpp"
#include "keys.hpp"
#include <FL/fl_ask.H>

using nlohmann::json;

const GLchar* vertex_src =
    "#version 330 core\n"
    "layout(location = 0) in vec2 position;\n"
    "layout(location = 1) in vec2 texCoord;\n"
    "out vec2 TexCoord;\n"
    "uniform mat4 projection;\n"
    "uniform vec2 user_pos;"
    "void main() {\n"
    "    TexCoord = texCoord;\n"
    "    gl_Position = projection * vec4(position + user_pos, 0.0, 1.0);\n"
    "}\n";

const GLchar* fragment_src =
    "#version 330 core\n"
    "in vec2 TexCoord;\n"
    "out vec4 outColor;\n"
    "uniform sampler2D tex;\n"
    "void main() {\n"
    "    outColor = texture(tex, TexCoord);\n"
    "}\n";

void VideoWindow::letterbox(int& x, int& y, int& width, int& height) const {
    std::lock_guard<std::mutex> lock(video.mutex);

    int window_width;
    int window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    float video_aspect_ratio = (float) video.width / video.height;
    float window_aspect_ratio = (float) window_width / window_height;
    if (video_aspect_ratio > window_aspect_ratio) {
        x = 0;
        width = window_width;
        height = ((float) window_width / video.width) * video.height;
        y = (window_height - height) / 2;
    } else if (video_aspect_ratio < window_aspect_ratio) {
        y = 0;
        width = ((float) window_height / video.height) * video.width;
        height = window_height;
        x = (window_width - width) / 2;
    } else {
        x = 0;
        y = 0;
        width = window_width;
        height = window_height;
    }
}

std::array<GLfloat, 16> VideoWindow::orthographic_matrix() const {
    int window_width;
    int window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    GLfloat c0r0 = 2.f / window_width;
    GLfloat c0r1 = 0.f;
    GLfloat c0r2 = 0.f;
    GLfloat c0r3 = 0.f;
    GLfloat c1r0 = 0.f;
    GLfloat c1r1 = 2.f / -window_height;
    GLfloat c1r2 = 0.f;
    GLfloat c1r3 = 0.f;
    GLfloat c2r0 = 0.f;
    GLfloat c2r1 = 0.f;
    GLfloat c2r2 = -2.f / 1.f;
    GLfloat c2r3 = 0.f;
    GLfloat c3r0 = (GLfloat) -window_width / window_width;
    GLfloat c3r1 = (GLfloat) -window_height / -window_height;
    GLfloat c3r2 = -1.f;
    GLfloat c3r3 = 1.f;
    // clang-format off
    return {
        c0r0, c0r1, c0r2, c0r3,
        c1r0, c1r1, c1r2, c1r3,
        c2r0, c2r1, c2r2, c2r3,
        c3r0, c3r1, c3r2, c3r3,
    };
    // clang-format on
}

void VideoWindow::window_pos_to_video_pos(int x, int y, int& x_ret, int& y_ret) const {
    int window_width;
    int window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    std::lock_guard<std::mutex> lock(video.mutex);

    float video_aspect_ratio = (float) video.width / video.height;
    float window_aspect_ratio = (float) window_width / window_height;
    if (video_aspect_ratio > window_aspect_ratio) {
        x_ret = x / ((float) window_width / video.width);
        y_ret = (y - ((1.f - window_aspect_ratio / video_aspect_ratio) * window_height / 2.f)) / ((float) window_width / video.width);
    } else if (video_aspect_ratio < window_aspect_ratio) {
        x_ret = (x - ((1.f - video_aspect_ratio / window_aspect_ratio) * window_width / 2.f)) / ((float) window_height / video.height);
        y_ret = y / ((float) window_height / video.height);
    } else {
        x_ret = x / ((float) window_width / video.width);
        y_ret = y / ((float) window_height / video.height);
    }
}

void VideoWindow::run(std::shared_ptr<rtc::PeerConnection> conn, std::shared_ptr<rtc::DataChannel> ordered_channel, std::shared_ptr<rtc::DataChannel> unordered_channel) {
    SDL_GL_MakeCurrent(window, gl_context);

    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    std::cout << "Graphics hardware info: " << vendor << ", " << renderer << std::endl;

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_src, nullptr);
    glCompileShader(vertex_shader);

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_src, nullptr);
    glCompileShader(fragment_shader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glUseProgram(program);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    video.mutex.lock();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, video.width, video.height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    video.mutex.unlock();

    int x;
    int y;
    int width;
    int height;
    letterbox(x, y, width, height);

    {
        GLuint loc = glGetUniformLocation(program, "user_pos");
        glUniform2f(loc, x, y);
    }

    auto matrix = orthographic_matrix();
    {
        GLuint loc = glGetUniformLocation(program, "projection");
        glUniformMatrix4fv(loc, 1, GL_FALSE, matrix.data());
    }

    {
        GLuint loc = glGetUniformLocation(program, "tex");
        glUniform1i(loc, 0);
    }

    GLuint vao;
    GLuint vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);

    std::array<GLfloat, 16> vertices = {
        0.f,
        0.f,
        0.f,
        0.f,
        (GLfloat) width,
        0.f,
        1.f,
        0.f,
        (GLfloat) width,
        (GLfloat) height,
        1.f,
        1.f,
        0.f,
        (GLfloat) height,
        0.f,
        1.f,
    };

    // clang-format off
    GLuint indices[] = {
        0, 1, 2,
        0, 2, 3,
    };
    // clang-format on

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_DYNAMIC_DRAW);

    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (GLvoid*) (sizeof(GLfloat) * 2));
    glEnableVertexAttribArray(1);

    glClearColor(0.f, 0.f, 0.f, 1.f);

    for (bool running = true; running; SDL_GL_SwapWindow(window)) {
        if (conn->iceState() == rtc::PeerConnection::IceState::Closed ||
            conn->iceState() == rtc::PeerConnection::IceState::Disconnected ||
            conn->iceState() == rtc::PeerConnection::IceState::Failed) {
            fl_alert("The connection has closed.");
            break;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                goto cleanup;

            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                case SDL_WINDOWEVENT_FOCUS_LOST: {
                    if (system("qdbus org.kde.kglobalaccel /kglobalaccel blockGlobalShortcuts false") != 0) {
                        std::cerr << "Warning: Qt D-Bus call failed" << std::endl;
                    }
                    break;
                }

                case SDL_WINDOWEVENT_FOCUS_GAINED: {
                    if (system("qdbus org.kde.kglobalaccel /kglobalaccel blockGlobalShortcuts true") != 0) {
                        std::cerr << "Warning: Qt D-Bus call failed" << std::endl;
                    }
                    break;
                }

                case SDL_WINDOWEVENT_RESIZED: {
                    letterbox(x, y, width, height);

                    {
                        GLuint loc = glGetUniformLocation(program, "user_pos");
                        glUniform2f(loc, x, y);
                    }

                    matrix = orthographic_matrix();
                    {
                        GLuint loc = glGetUniformLocation(program, "projection");
                        glUniformMatrix4fv(loc, 1, GL_FALSE, matrix.data());
                    }

                    vertices = {
                        0.f,
                        0.f,
                        0.f,
                        0.f,
                        (GLfloat) width,
                        0.f,
                        1.f,
                        0.f,
                        (GLfloat) width,
                        (GLfloat) height,
                        1.f,
                        1.f,
                        0.f,
                        (GLfloat) height,
                        0.f,
                        1.f,
                    };
                    glBindBuffer(GL_ARRAY_BUFFER, vbo);
                    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_DYNAMIC_DRAW);
                    glViewport(0, 0, event.window.data1, event.window.data2);

                    break;
                }
                }
                break;

            case SDL_KEYDOWN:
                if (!event.key.repeat &&
                    event.key.keysym.sym != SDLK_F10 &&
                    event.key.keysym.sym != SDLK_F11 &&
                    ordered_channel->isOpen()) {
                    json message = {
                        {"type", "keydown"},
                        {"key", sdl_to_browser_key(event.key.keysym.sym)},
                    };
                    ordered_channel->send(message.dump());
                }
                break;

            case SDL_KEYUP:
                if (event.key.repeat) {
                    break;
                }

                if (event.key.keysym.sym == SDLK_F9) {
                    if (!client_side_mouse) SDL_SetRelativeMouseMode(SDL_GetRelativeMouseMode() ? SDL_FALSE : SDL_TRUE);
                    SDL_SetWindowKeyboardGrab(window, SDL_GetWindowKeyboardGrab(window) ? SDL_FALSE : SDL_TRUE);
                } else if (event.key.keysym.sym == SDLK_F11) {
                    Uint32 flags = SDL_GetWindowFlags(window);
                    if (flags & SDL_WINDOW_FULLSCREEN) {
                        SDL_SetWindowFullscreen(window, 0);
                    } else {
                        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                } else if (ordered_channel->isOpen()) {
                    json message = {
                        {"type", "keyup"},
                        {"key", sdl_to_browser_key(event.key.keysym.sym)},
                    };
                    ordered_channel->send(message.dump());
                }
                break;

            case SDL_MOUSEMOTION:
                if (client_side_mouse) {
                    if (ordered_channel->isOpen()) {
                        int x;
                        int y;
                        window_pos_to_video_pos(event.motion.x, event.motion.y, x, y);

                        json message = {
                            {"type", "mousemoveabs"},
                            {"x", x},
                            {"y", y},
                        };
                        ordered_channel->send(message.dump());
                    }
                } else {
                    if (unordered_channel->isOpen()) {
                        json message = {
                            {"type", "mousemove"},
                            {"x", event.motion.xrel},
                            {"y", event.motion.yrel},
                        };
                        unordered_channel->send(message.dump());
                    }
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (ordered_channel->isOpen()) {
                    json message = {
                        {"type", "mousedown"},
                        {"button", event.button.button - 1},
                    };
                    ordered_channel->send(message.dump());
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (ordered_channel->isOpen()) {
                    json message = {
                        {"type", "mouseup"},
                        {"button", event.button.button - 1},
                    };
                    ordered_channel->send(message.dump());
                }
                break;

            case SDL_MOUSEWHEEL:
                if (unordered_channel->isOpen()) {
                    json message = {
                        {"type", "wheel"},
                        {"x", event.wheel.x * 120},
                        {"y", event.wheel.y * -120},
                    };
                    unordered_channel->send(message.dump());
                }
                break;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);

        video.mutex.lock();
        if (video.sample) {
            GstBuffer* buf = gst_sample_get_buffer(video.sample);
            GstMapInfo map;
            gst_buffer_map(buf, &map, GST_MAP_READ);
            if (video.resized) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, video.width, video.height, 0, GL_RGB, GL_UNSIGNED_BYTE, map.data);
                video.resized = false;
            } else {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, video.width, video.height, GL_RGB, GL_UNSIGNED_BYTE, map.data);
            }
            gst_buffer_unmap(buf, &map);
        }
        video.mutex.unlock();
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    }

cleanup:
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteTextures(1, &texture);
    glDeleteProgram(program);
}
