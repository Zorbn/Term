#include "detect_leak.h"

#include "window.h"
#include "graphics/resources.h"
#include "graphics/sprite_batch.h"

#include <cglm/struct.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

const float sky_color_r = 50.0f / 255.0f;
const float sky_color_g = 74.0f / 255.0f;
const float sky_color_b = 117.0f / 255.0f;

const int32_t grid_width = 80;
const int32_t grid_height = 24;

typedef struct {
    HRESULT result;
    HANDLE output, input;
} PseudoConsole;

typedef struct {
    wchar_t *text;
    UINT32 textLength;
    RECT rc;
} Data;

bool data_match_char(Data *data, wchar_t character, size_t *i) {
    if (*i >= data->textLength) {
        return false;
    }

    if (data->text[*i] != character) {
        return false;
    }

    *i += 1;
    return true;
}

bool data_peek_digit(Data *data, size_t i) {
    if (i >= data->textLength) {
        return false;
    }

    return iswdigit(data->text[i]);
}

void data_destroy(Data *data) {
    free(data->text);
}

HRESULT PrepareStartupInformation(HPCON hpc, STARTUPINFOEX *psi) {
    // Prepare Startup Information structure
    STARTUPINFOEX si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEX);

    // Discover the size required for the list
    size_t bytesRequired;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);

    // Allocate memory to represent the list
    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytesRequired);
    if (!si.lpAttributeList) {
        return E_OUTOFMEMORY;
    }

    // Initialize the list memory location
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired)) {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Set the pseudoconsole information into the list
    if (!UpdateProcThreadAttribute(
            si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof(hpc), NULL, NULL)) {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    *psi = si;

    return S_OK;
}

PseudoConsole SetUpPseudoConsole(COORD size) {
    HRESULT hr = S_OK;

    // - Close these after CreateProcess of child application with
    // pseudoconsole object.
    HANDLE inputReadSide, outputWriteSide;

    // - Hold onto these and use them for communication with the child
    // through the pseudoconsole.
    HANDLE outputReadSide, inputWriteSide;

    if (!CreatePipe(&inputReadSide, &inputWriteSide, NULL, 0)) {
        return (PseudoConsole){
            HRESULT_FROM_WIN32(GetLastError()),
            NULL,
            NULL,
        };
    }

    if (!CreatePipe(&outputReadSide, &outputWriteSide, NULL, 0)) {
        return (PseudoConsole){
            HRESULT_FROM_WIN32(GetLastError()),
            NULL,
            NULL,
        };
    }

    HPCON hPC;
    hr = CreatePseudoConsole(size, inputReadSide, outputWriteSide, 0, &hPC);
    if (FAILED(hr)) {
        return (PseudoConsole){
            hr,
            NULL,
            NULL,
        };
    }

    STARTUPINFOEX siEx;
    PrepareStartupInformation(hPC, &siEx);

    // PCWSTR childApplication = L"C:\\windows\\system32\\cmd.exe";
    PCWSTR childApplication = L"C:\\Program Files\\PowerShell\\7\\pwsh.exe -c \"python test.py\"";
    // PCWSTR childApplication = L"python test.py";

    // Create mutable text string for CreateProcessW command line string.
    const size_t charsRequired = wcslen(childApplication) + 1; // +1 null terminator
    PWSTR cmdLineMutable = (PWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * charsRequired);

    if (!cmdLineMutable) {
        return (PseudoConsole){
            E_OUTOFMEMORY,
            NULL,
            NULL,
        };
    }

    wcscpy_s(cmdLineMutable, charsRequired, childApplication);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Call CreateProcess
    if (!CreateProcessW(NULL, cmdLineMutable, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
            (LPSTARTUPINFOW)(&siEx.StartupInfo), &pi)) {
        HeapFree(GetProcessHeap(), 0, cmdLineMutable);
        return (PseudoConsole){
            HRESULT_FROM_WIN32(GetLastError()),
            NULL,
            NULL,
        };
    }

    return (PseudoConsole){
        hr,
        outputReadSide,
        inputWriteSide,
    };
}

struct Grid {
    wchar_t *data;
    size_t width;
    size_t height;
    size_t size;

    int32_t cursor_x;
    int32_t cursor_y;
    bool show_cursor;
};

struct Grid grid_create(size_t width, size_t height) {
    size_t size = width * height;
    struct Grid grid = (struct Grid){
        .data = malloc(grid_width * grid_height * sizeof(wchar_t)),
        .width = width,
        .height = height,
        .size = size,
    };
    assert(grid.data);

    for (size_t i = 0; i < size; i++) {
        grid.data[i] = L' ';
    }

    return grid;
}

void grid_destroy(struct Grid *grid) {
    free(grid->data);
}

void grid_scroll_down(struct Grid *grid) {
    memmove(&grid->data[0], &grid->data[grid->width], (grid->size - grid->width) * sizeof(wchar_t));

    size_t new_row_start_i = (grid->height - 1) * grid->width;
    for (size_t i = 0; i < grid->width; i++) {
        grid->data[i + new_row_start_i] = L' ';
    }

    grid->cursor_y--;
}

void grid_cursor_move_to(struct Grid *grid, int32_t x, int32_t y, bool can_scroll) {
    grid->cursor_x = x;
    grid->cursor_y = y;

    if (grid->cursor_x < 0) {
        grid->cursor_x = 0;
    }

    if (grid->cursor_y < 0) {
        grid->cursor_y = 0;
    }

    // TODO: This is a hack, we are tying the visible cursor position and the actual cursor position together.
    // That will cause problems when implementing resizing, scrollback, etc. Maybe replace this with proper line wrap
    // later.
    while (grid->cursor_x >= grid->width) {
        if (can_scroll) {
            grid->cursor_x -= grid->width;
            grid->cursor_y++;
        } else {
            grid->cursor_x = grid->width - 1;
        }
    }

    while (grid->cursor_y >= grid->height) {
        if (can_scroll) {
            grid_scroll_down(grid);
        } else {
            grid->cursor_y = grid->height - 1;
        }
    }
}

void grid_cursor_move(struct Grid *grid, int32_t delta_x, int32_t delta_y, bool can_scroll) {
    grid_cursor_move_to(grid, grid->cursor_x + delta_x, grid->cursor_y + delta_y, can_scroll);
}

// Returns true if an escape sequence was parsed.
bool grid_parse_escape_sequence(struct Grid *grid, Data *data, size_t *i, struct Window *window) {
    size_t start_i = *i;
    if (!data_match_char(data, L'\x1b', i)) {
        *i = start_i;
        return false;
    }

    // Operating system commands:
    if (data_match_char(data, L']', i)) {
        bool has_zero_or_two = data_match_char(data, L'0', i) || data_match_char(data, L'2', i);
        if (!has_zero_or_two || !data_match_char(data, L';', i)) {
            *i = start_i;
            return false;
        }

        // Window titles can be at most 255 characters.
        for (size_t title_length = 0; title_length < 255; title_length++) {
            size_t peek_i = *i + title_length;
            bool has_bel = data_match_char(data, L'\x7', &peek_i);
            bool has_terminator =
                has_bel || (data_match_char(data, L'\x1b', &peek_i) && data_match_char(data, L'\x5c', &peek_i));
            if (!has_terminator) {
                continue;
            }

            size_t title_string_length = title_length + 1;
            char *title = malloc(title_string_length);
            wcstombs_s(NULL, title, title_string_length, data->text + *i, title_length);

            glfwSetWindowTitle(window->glfw_window, (char *)title);

            free(title);

            *i = peek_i;
            return true;
        }
    }

    // Control sequence introducers:
    if (data_match_char(data, L'[', i)) {
        // Cursor visibility:
        if (data_match_char(data, L'?', i)) {
            if (data_match_char(data, L'2', i) && data_match_char(data, L'5', i)) {
                if (data_match_char(data, L'h', i)) {
                    grid->show_cursor = true;
                    return true;
                }

                if (data_match_char(data, L'l', i)) {
                    grid->show_cursor = false;
                    return true;
                }
            }

            *i = start_i;
            return false;
        }

        // TODO: Make sure when parsing sequences that they don't have more numbers supplied then they allow, ie: no
        // ESC[10;5A because A should only accept one number.
        size_t parsed_number_count = 0;
        int32_t number1 = 0;
        int32_t number2 = 0;

        // TODO: Make this into a function.
        while (data_peek_digit(data, *i)) {
            parsed_number_count = 1;
            int32_t digit = data->text[*i] - L'0';
            number1 = number1 * 10 + digit;
            *i += 1;
        }

        if (data_match_char(data, L';', i)) {
            while (data_peek_digit(data, *i)) {
                parsed_number_count = 2;
                int32_t digit = data->text[*i] - L'0';
                number2 = number2 * 10 + digit;
                *i += 1;
            }
        }

        // Text formatting:
        {
            if (data_match_char(data, L'm', i)) {
                // TODO: Change format.
                return true;
            }
        }

        // Cursor positioning:
        {
            if (parsed_number_count < 2) {
                int32_t n = parsed_number_count > 0 ? number1 : 1;

                // Up:
                if (data_match_char(data, L'A', i)) {
                    grid_cursor_move(grid, 0, -n, false);
                    return true;
                }

                // Down:
                if (data_match_char(data, L'B', i)) {
                    grid_cursor_move(grid, 0, n, false);
                    return true;
                }

                // Backward:
                if (data_match_char(data, L'D', i)) {
                    grid_cursor_move(grid, -n, 0, false);
                    return true;
                }

                // Forward:
                if (data_match_char(data, L'C', i)) {
                    grid_cursor_move(grid, n, 0, false);
                    return true;
                }
            }

            int32_t x = parsed_number_count > 0 ? number1 : 1;
            int32_t y = parsed_number_count > 1 ? number2 : 1;

            // Cursor position or horizontal vertical position:
            if (data_match_char(data, L'H', i) || data_match_char(data, L'f', i)) {
                if (x > 0 && y > 0) {
                    grid_cursor_move_to(grid, x - 1, y - 1, false);
                }
                return true;
            }
        }

        // Text modification:
        {
            int32_t n = parsed_number_count > 0 ? number1 : 0;

            switch (n) {
                case 2: {
                    if (data_match_char(data, L'J', i)) {
                        // Earase entire display.
                        for (size_t y = 0; y < grid->height; y++) {
                            for (size_t x = 0; x < grid->width; x++) {
                                grid->data[x + y * grid->width] = L' ';
                            }
                        }

                        return true;
                    }

                    break;
                }
            }
        }
    }

    // This sequence is invalid, ignore it.
    *i = start_i;
    return false;
}

void grid_draw_character(struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z,
    int32_t origin_y, float r, float g, float b) {

    wchar_t character = grid->data[x + y * grid->width];
    if (character < 33 || character > 126) {
        return;
    }

    sprite_batch_add(sprite_batch, (struct Sprite){
                                       .x = x * 6,
                                       .y = origin_y - (y + 1) * 14,
                                       .z = z,
                                       .width = 6,
                                       .height = 14,

                                       .texture_x = 8 * (character - 32),
                                       .texture_width = 6,
                                       .texture_height = 14,

                                       .r = r,
                                       .g = g,
                                       .b = b,
                                   });
}

void grid_draw_cursor(
    struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z, int32_t origin_y) {
    sprite_batch_add(sprite_batch, (struct Sprite){
                                       .x = x * 6,
                                       .y = origin_y - (y + 1) * 14,
                                       .z = z,
                                       .width = 6,
                                       .height = 14,

                                       .texture_x = 0,
                                       .texture_width = 6,
                                       .texture_height = 14,

                                       .r = 1.0f,
                                       .g = 1.0f,
                                       .b = 1.0f,
                                   });

    grid_draw_character(grid, sprite_batch, x, y, z + 1, origin_y, 0.0f, 0.0f, 0.0f);
}

int main() {
    struct Window window = window_create("CBlock", 640, 480);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    uint32_t program_2d = program_create("assets/shader_2d.vert", "assets/shader_2d.frag");

    // TODO: Add texture_bind and texture_destroy
    struct Texture texture_atlas_2d = texture_create("assets/texture_atlas.png");

    struct SpriteBatch sprite_batch = sprite_batch_create(16);

    mat4s projection_matrix_2d;

    int32_t projection_matrix_location_2d = glGetUniformLocation(program_2d, "projection_matrix");

    Data data = {0};
    data.text = NULL;
    data.textLength = 0;
    PseudoConsole console = SetUpPseudoConsole((COORD){grid_width, grid_height});
    printf("Console result: %ld\n", console.result);
    struct Grid grid = grid_create(grid_width, grid_height);

    double last_frame_time = glfwGetTime();
    float fps_print_timer = 0.0f;

    float elapsed_time = 0.0f;

    while (!glfwWindowShouldClose(window.glfw_window)) {
        // Update:
        if (window.was_resized) {
            glViewport(0, 0, window.width, window.height);
            projection_matrix_2d = glms_ortho(0.0f, (float)window.width, 0.0f, (float)window.height, -100.0, 100.0);

            window.was_resized = false;
        }

        double current_frame_time = glfwGetTime();
        float delta_time = (float)(current_frame_time - last_frame_time);
        last_frame_time = current_frame_time;

        fps_print_timer += delta_time;

        if (fps_print_timer > 1.0f) {
            fps_print_timer = 0.0f;
            printf("fps: %f\n", 1.0f / delta_time);
        }

        elapsed_time += delta_time;
        float time_of_day = 0.5f * (sin(elapsed_time * 0.1f) + 1.0f);

        sprite_batch_begin(&sprite_batch);

        // START HANDLE TERMINAL

        DWORD bytesAvailable;
        CHAR chBuf[1024];
        PeekNamedPipe(console.output, NULL, 0, NULL, &bytesAvailable, NULL);

        if (bytesAvailable > 0) {
            int32_t bytesToRead = 1024;
            DWORD dwRead;
            BOOL success = ReadFile(console.output, chBuf, bytesToRead, &dwRead, NULL);
            if (success && dwRead != 0) {
                data_destroy(&data);
                data.text = (wchar_t *)malloc((dwRead + 1) * sizeof(wchar_t));
                assert(data.text);
                size_t out;
                mbstowcs_s(&out, data.text, dwRead + 1, chBuf, dwRead);
                data.textLength = dwRead;

                for (size_t i = 0; i < data.textLength;) {
                    // Check for escape sequences.
                    // https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences If <n>
                    // is omitted for colors, it is assumed to be 0, if <x,y,n> are omitted for positioning, they
                    // are assumed to be 1.
                    if (grid_parse_escape_sequence(&grid, &data, &i, &window)) {
                        continue;
                    }

                    if (data_match_char(&data, L'\r', &i)) {
                        grid_cursor_move_to(&grid, 0, grid.cursor_y + 1, true);
                        continue;
                    }

                    if (data_match_char(&data, L'\n', &i)) {
                        continue;
                    }

                    grid.data[grid.cursor_x + grid.cursor_y * grid_width] = data.text[i];
                    grid_cursor_move(&grid, 1, 0, true);

                    i++;
                }
            }
        }

        for (size_t y = 0; y < grid_height; y++) {
            for (size_t x = 0; x < grid_width; x++) {
                grid_draw_character(&grid, &sprite_batch, x, y, 0, window.height, 1.0f, 1.0f, 1.0f);
            }
        }

        if (grid.show_cursor) {
            grid_draw_cursor(&grid, &sprite_batch, grid.cursor_x, grid.cursor_y, 1, window.height);
        }

        // END HANDLE TERMINAL

        sprite_batch_end(&sprite_batch, texture_atlas_2d.width, texture_atlas_2d.height);

        // Draw:
        glClearColor(sky_color_r * time_of_day, sky_color_g * time_of_day, sky_color_b * time_of_day, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program_2d);
        glUniformMatrix4fv(projection_matrix_location_2d, 1, GL_FALSE, (const float *)&projection_matrix_2d);
        glBindTexture(GL_TEXTURE_2D, texture_atlas_2d.id);
        sprite_batch_draw(&sprite_batch);

        window_update(&window);

        glfwSwapBuffers(window.glfw_window);
        glfwPollEvents();
    }

    grid_destroy(&grid);
    data_destroy(&data);

    sprite_batch_destroy(&sprite_batch);

    glDeleteTextures(1, &texture_atlas_2d.id);

    window_destroy(&window);

    printf("Found leaks: %s\n", _CrtDumpMemoryLeaks() ? "true" : "false");

    return 0;
}
